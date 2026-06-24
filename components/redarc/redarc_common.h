#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/canbus/canbus.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace redarc_common {

static const char *const TAG = "redarc_common";

inline void log_can_frame(const char *direction, uint32_t can_id, const std::vector<uint8_t> &data, bool rtr = false) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  const uint32_t rvc_id = can_id & 0x1FFFFFFFUL;
  const uint32_t dgn = (rvc_id >> 8) & 0x1FFFFUL;
  const uint8_t sa = (uint8_t) (rvc_id & 0xFFU);
  const uint8_t d1 = data.size() > 0 ? data[0] : 0xFF;
  const uint8_t d2 = data.size() > 1 ? data[1] : 0xFF;
  const uint8_t d3 = data.size() > 2 ? data[2] : 0xFF;
  const uint8_t d4 = data.size() > 3 ? data[3] : 0xFF;
  const uint8_t d5 = data.size() > 4 ? data[4] : 0xFF;
  const uint8_t d6 = data.size() > 5 ? data[5] : 0xFF;
  const uint8_t d7 = data.size() > 6 ? data[6] : 0xFF;
  const uint8_t d8 = data.size() > 7 ? data[7] : 0xFF;
  ESP_LOGD(TAG, "%s id=0x%08X dgn=0x%05X sa=0x%02X rtr=%u dlc=%u data=%02X %02X %02X %02X %02X %02X %02X %02X",
           direction, (unsigned) rvc_id, (unsigned) dgn, (unsigned) sa, (unsigned) rtr, (unsigned) data.size(),
           (unsigned) d1, (unsigned) d2, (unsigned) d3, (unsigned) d4,
           (unsigned) d5, (unsigned) d6, (unsigned) d7, (unsigned) d8);
#endif
}

class RedarcCanDispatcher {
 public:
  static RedarcCanDispatcher &instance() {
    static RedarcCanDispatcher inst;
    return inst;
  }
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  canbus::Canbus *canbus() { return this->canbus_; }
  void add_listener(std::function<void(uint32_t, const std::vector<uint8_t> &)> cb) {
    this->listeners_.push_back(std::move(cb));
  }
  void dispatch(uint32_t can_id, const std::vector<uint8_t> &data, bool rtr = false) {
    log_can_frame("CAN_RX", can_id, data, rtr);
    if (rtr) return;
    for (auto &cb : this->listeners_) cb(can_id, data);
  }
 private:
  canbus::Canbus *canbus_{nullptr};
  std::vector<std::function<void(uint32_t, const std::vector<uint8_t> &)>> listeners_;
};

class RedarcCommonComponent : public Component {
 public:
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_host_address(uint8_t host_address) { this->host_address_ = host_address; }

  void setup() override {
    RedarcCanDispatcher::instance().set_canbus(this->canbus_);

    if (this->canbus_ == nullptr) {
      ESP_LOGE(TAG, "CAN bus is unavailable");
      return;
    }

    // Dispatch every received frame to the device listeners directly from the
    // component (no YAML on_frame: automation needed), and passively collect
    // device-identity frames so the bus can be enumerated.
    this->canbus_->add_callback(
        [this](uint32_t can_id, bool /*extended_id*/, bool rtr, const std::vector<uint8_t> &data) {
          const uint32_t id = can_id & 0x1FFFFFFFUL;
          RedarcCanDispatcher::instance().dispatch(id, data, rtr);
          if (rtr) return;
          this->handle_device_identity_(id, data);
        });

#if defined(USE_API) && defined(USE_API_CLIENT_CONNECTED_TRIGGER)
    // Print the discovered-device list once whenever an API client (e.g. the
    // dashboard log viewer) connects. The macro is enabled from our codegen, so
    // no on_client_connected: YAML is needed. Leaks intentionally — lives forever.
    if (api::global_api_server != nullptr) {
      auto *automation =
          new Automation<std::string, std::string>(api::global_api_server->get_client_connected_trigger());
      automation->add_action(new LambdaAction<std::string, std::string>(
          [this](const std::string &, const std::string &) { this->on_logger_connected_(); }));
    }
#endif
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Redarc common:");
    ESP_LOGCONFIG(TAG, "  Passive device discovery (DGN 0x1F404): enabled");
  }

  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  struct DiscoveredDevice {
    uint8_t source_address;
    uint8_t device_type;
    uint32_t serial_prefix;
    uint16_t serial_suffix;
  };

  // DeviceType codes carried in D7 of DGN 0x1F404 (per the REDARC DBC).
  static const char *device_type_name_(uint8_t device_type) {
    switch (device_type) {
      case 1: return "Manager30";
      case 3: return "BMS Battery Sensor";
      case 12: return "RedVision Display";
      case 14: return "TVMS1280";
      case 22: return "TVMS Rogue";
      default: return nullptr;
    }
  }

  // Passive device discovery. Every REDARC device broadcasts a DGN 0x1F404
  // identity frame every couple of seconds, so the whole bus is enumerated just
  // by listening — no request is sent. Devices are collected silently here and
  // printed once when a logger connects (see on_logger_connected_()).
  // Layout: D1-D4 serial prefix (uint32 LE), D5-D6 serial suffix (uint16 LE),
  // D7 device type, D8 subtype/family.
  void handle_device_identity_(uint32_t id, const std::vector<uint8_t> &data) {
    if ((((id >> 8) & 0x1FFFFUL) != 0x1F404UL) || data.size() < 8) return;

    const uint8_t source_address = (uint8_t) (id & 0xFFU);
    for (auto &d : this->devices_)
      if (d.source_address == source_address) return;  // already known

    DiscoveredDevice dev;
    dev.source_address = source_address;
    dev.serial_prefix = ((uint32_t) data[0]) | ((uint32_t) data[1] << 8) |
                        ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
    dev.serial_suffix = (uint16_t) data[4] | ((uint16_t) data[5] << 8);
    dev.device_type = data[6];
    this->devices_.push_back(dev);
  }

  // Print the discovered devices once. A short delay (re-scheduled on each
  // connect, so it fires only once) lets the list fill in and the log stream
  // settle before we print.
  void on_logger_connected_() {
    this->set_timeout("redarc_discovery_print", 3000, [this]() { this->log_discovered_devices_(); });
  }

  void log_discovered_devices_() {
    std::sort(this->devices_.begin(), this->devices_.end(),
              [](const DiscoveredDevice &a, const DiscoveredDevice &b) {
                return a.source_address < b.source_address;
              });

    // Width the Device Type column to the longest name so the columns line up.
    size_t type_w = sizeof("Device Type") - 1;
    for (auto &d : this->devices_) {
      const char *name = device_type_name_(d.device_type);
      const size_t len = (name != nullptr) ? std::strlen(name) : sizeof("Unknown (0xFF)") - 1;
      if (len > type_w) type_w = len;
    }

    ESP_LOGI(TAG, "Discovered devices:");
    ESP_LOGI(TAG, "  %-*s  %-10s  %s", (int) type_w, "Device Type", "Address", "Serial No");
    for (auto &d : this->devices_) {
      const char *name = device_type_name_(d.device_type);
      char type_buf[24];
      if (name == nullptr) {
        std::snprintf(type_buf, sizeof(type_buf), "Unknown (0x%02X)", (unsigned) d.device_type);
        name = type_buf;
      }
      char addr_buf[16];
      std::snprintf(addr_buf, sizeof(addr_buf), "%u (0x%02X)", (unsigned) d.source_address,
                    (unsigned) d.source_address);
      ESP_LOGI(TAG, "  %-*s  %-10s  %010lu-%04u", (int) type_w, name, addr_buf,
               (unsigned long) d.serial_prefix, (unsigned) d.serial_suffix);
    }
  }

  canbus::Canbus *canbus_{nullptr};
  uint8_t host_address_{0x22};
  std::vector<DiscoveredDevice> devices_;
};

inline uint16_t u16_le(const std::vector<uint8_t> &data, uint8_t i) {
  if (data.size() <= i + 1) return 0;
  return (uint16_t) data[i] | ((uint16_t) data[i + 1] << 8);
}

inline uint32_t u32_le(const std::vector<uint8_t> &data, uint8_t i) {
  if (data.size() <= i + 3) return 0;
  return ((uint32_t) data[i]) |
         ((uint32_t) data[i + 1] << 8) |
         ((uint32_t) data[i + 2] << 16) |
         ((uint32_t) data[i + 3] << 24);
}

inline uint32_t with_sa(uint32_t id_base, uint8_t source_address) {
  return (id_base & 0x1FFFFF00UL) | source_address;
}

inline uint32_t rvc_id(uint32_t can_id) {
  return can_id & 0x1FFFFFFFUL;
}

inline uint32_t rvc_dgn(uint32_t can_id) {
  return (rvc_id(can_id) >> 8) & 0x1FFFFUL;
}

inline uint8_t rvc_source_address(uint32_t can_id) {
  return (uint8_t) (rvc_id(can_id) & 0xFFU);
}

inline bool rvc_matches(uint32_t can_id, uint32_t dgn, uint8_t source_address) {
  return rvc_dgn(can_id) == dgn && rvc_source_address(can_id) == source_address;
}

inline float current_32_centered(uint32_t raw) {
  return ((float) raw / 1000.0f) - 1000.0f;
}

inline float current_display_16_centered(uint16_t raw) {
  return ((float) raw / 10.0f) - 1000.0f;
}

inline const char *output_status_name(uint8_t status) {
  switch (status) {
    case 0x06: return "Fuse Blown";
    case 0x0A: return "Over Temp";
    case 0x14: return "Off Override";
    case 0x15: return "On Override";
    case 0xF8: return "Unconfigured";
    default: return nullptr;
  }
}

inline void send_command(uint32_t can_id, const std::vector<uint8_t> &data, bool rtr = false) {
  auto *bus = RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  log_can_frame("CAN_TX", can_id, data, rtr);
  bus->send_data(can_id, true, rtr, data);
}

inline void send_clear_history(uint8_t host_address, uint8_t dataset) {
  const std::vector<uint8_t> data = {dataset, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  send_command(0x1BFCCE00UL | host_address, data);
}

}  // namespace redarc_common
}  // namespace esphome
