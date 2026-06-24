#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/canbus/canbus.h"

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
  void set_discovery_delay_ms(uint32_t discovery_delay_ms) { this->discovery_delay_ms_ = discovery_delay_ms; }

  void setup() override {
    RedarcCanDispatcher::instance().set_canbus(this->canbus_);

    if (this->canbus_ == nullptr) {
      ESP_LOGE(TAG, "CAN bus is unavailable; device discovery disabled");
      return;
    }

    this->canbus_->add_callback(
        [this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
          if (!extended_id || rtr) return;
          this->handle_device_identity_(can_id, data);
        });

    this->set_timeout("redarc_device_discovery_1", this->discovery_delay_ms_, [this]() {
      this->request_all_devices_();
    });
    this->set_timeout("redarc_device_discovery_2", this->discovery_delay_ms_ + 3000U, [this]() {
      this->request_all_devices_();
    });
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Redarc common:");
    ESP_LOGCONFIG(TAG, "  Startup device discovery: enabled");
    ESP_LOGCONFIG(TAG, "  Request source address: 0x%02X", (unsigned) this->host_address_);
  }

  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  static const char *device_type_name_(uint8_t device_type) {
    switch (device_type) {
      case 0x01: return "Manager30";
      case 0x03: return "BMS Battery Sensor";
      case 0x0C: return "RedVision Display";
      case 0x0E: return "TVMS 1280";
      case 0x16: return "TVMS Rouge";
      default: return nullptr;
    }
  }

  void request_all_devices_() {
    if (this->canbus_ == nullptr) return;

    const uint32_t can_id = 0x0F03FF00UL | this->host_address_;
    const std::vector<uint8_t> data = {0x04, 0xF4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    log_can_frame("CAN_TX", can_id, data, false);
    const auto error = this->canbus_->send_data(can_id, true, false, data);
    if (error != canbus::ERROR_OK) {
      ESP_LOGW(TAG, "Device discovery request failed with CAN error %u", (unsigned) error);
    } else {
      ESP_LOGD(TAG, "Requested DGN 0x1F404 from all devices using source address 0x%02X",
               (unsigned) this->host_address_);
    }
  }

  void handle_device_identity_(uint32_t can_id, const std::vector<uint8_t> &data) {
    const uint32_t id = can_id & 0x1FFFFFFFUL;
    const uint32_t dgn = (id >> 8) & 0x1FFFFUL;
    if (dgn != 0x1F404UL || data.size() < 8) return;

    const uint8_t source_address = (uint8_t) (id & 0xFFU);
    const uint32_t serial_prefix =
        ((uint32_t) data[0]) |
        ((uint32_t) data[1] << 8) |
        ((uint32_t) data[2] << 16) |
        ((uint32_t) data[3] << 24);
    const uint16_t serial_suffix =
        (uint16_t) data[4] | ((uint16_t) data[5] << 8);
    const uint8_t device_type = data[6];

    const uint64_t device_key =
        ((uint64_t) serial_prefix << 32) |
        ((uint64_t) serial_suffix << 16) |
        ((uint64_t) device_type << 8) |
        (uint64_t) source_address;

    if (std::find(this->seen_device_keys_.begin(), this->seen_device_keys_.end(), device_key) !=
        this->seen_device_keys_.end()) {
      return;
    }
    this->seen_device_keys_.push_back(device_key);

    const char *device_name = device_type_name_(device_type);
    if (device_name != nullptr) {
      ESP_LOGI(TAG, "Device Type: %s", device_name);
    } else {
      ESP_LOGI(TAG, "Device Type: Unknown (0x%02X)", (unsigned) device_type);
    }
    ESP_LOGI(TAG, "Address: %u", (unsigned) source_address);
    ESP_LOGI(TAG, "Serial No: %010lu-%04u", (unsigned long) serial_prefix, (unsigned) serial_suffix);
  }

  canbus::Canbus *canbus_{nullptr};
  uint8_t host_address_{0x22};
  uint32_t discovery_delay_ms_{2000};
  std::vector<uint64_t> seen_device_keys_;
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
