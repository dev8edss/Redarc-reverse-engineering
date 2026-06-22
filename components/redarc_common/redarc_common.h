#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/canbus/canbus.h"

namespace esphome {
namespace redarc_common {

static const uint32_t REDVISION_HEARTBEAT_INTERVAL_MS = 1000;

inline void log_can_frame(const char *direction, uint32_t can_id, const std::vector<uint8_t> &data) {
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
  ESP_LOGD("redarc_common", "%s id=0x%08X dgn=0x%05X sa=0x%02X dlc=%u data=%02X %02X %02X %02X %02X %02X %02X %02X",
           direction, (unsigned) rvc_id, (unsigned) dgn, (unsigned) sa, (unsigned) data.size(),
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
  void set_address_claim_sent(bool sent) { this->address_claim_sent_ = sent; }
  bool address_claim_sent() const { return this->address_claim_sent_; }
  void add_listener(std::function<void(uint32_t, const std::vector<uint8_t> &)> cb) {
    this->listeners_.push_back(std::move(cb));
  }
  void dispatch(uint32_t can_id, const std::vector<uint8_t> &data) {
    log_can_frame("CAN_RX", can_id, data);
    for (auto &cb : this->listeners_) cb(can_id, data);
  }
 private:
  canbus::Canbus *canbus_{nullptr};
  bool address_claim_sent_{false};
  std::vector<std::function<void(uint32_t, const std::vector<uint8_t> &)>> listeners_;
};

class RedarcCommonComponent : public Component {
 public:
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_host_address(uint8_t host_address) { this->host_address_ = host_address; }
  void setup() override {
    RedarcCanDispatcher::instance().set_canbus(this->canbus_);
    this->send_address_claim_();
  }
  void loop() override {
    if (!RedarcCanDispatcher::instance().address_claim_sent()) this->send_address_claim_();
    this->send_heartbeat_();
  }
  float get_setup_priority() const override { return setup_priority::BUS; }
 protected:
  void send_address_claim_() {
    if (this->canbus_ == nullptr) return;
    const uint32_t can_id = 0x03EEFF00UL | this->host_address_;
    const std::vector<uint8_t> data = {0x13, 0xCA, 0x23, 0x95, 0x0D, 0x00, 0x0C, 0x80};
    log_can_frame("CAN_TX", can_id, data);
    this->canbus_->send_data(can_id, true, data);
    RedarcCanDispatcher::instance().set_address_claim_sent(true);
    ESP_LOGD("redarc_common", "Sent address claim for SA 0x%02X", this->host_address_);
  }

  void send_heartbeat_() {
    if (!RedarcCanDispatcher::instance().address_claim_sent()) return;
    if (this->canbus_ == nullptr) return;
    const uint32_t now = millis();
    if (this->last_heartbeat_ms_ != 0 && now - this->last_heartbeat_ms_ < REDVISION_HEARTBEAT_INTERVAL_MS) return;
    this->last_heartbeat_ms_ = now;

    const uint32_t can_id = 0x1BF40400UL | this->host_address_;
    const std::vector<uint8_t> data = {0x13, 0xCA, 0x23, 0x95, 0x0E, 0x00, 0x0C, 0x02};
    log_can_frame("CAN_TX", can_id, data);
    this->canbus_->send_data(can_id, true, data);
  }

  canbus::Canbus *canbus_{nullptr};
  uint8_t host_address_{0x22};
  uint32_t last_heartbeat_ms_{0};
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

}  // namespace redarc_common
}  // namespace esphome
