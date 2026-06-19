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
  void dispatch(uint32_t can_id, const std::vector<uint8_t> &data) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    {
      const uint32_t rvc_id = can_id & 0x1FFFFFFFUL;
      const uint32_t dgn = (rvc_id >> 8) & 0x1FFFFUL;
      const uint8_t sa = (uint8_t) (rvc_id & 0xFFU);
      char hex[25] = {};
      size_t n = data.size() < 8 ? data.size() : 8;
      for (size_t i = 0; i < n; i++) snprintf(hex + i * 3, 4, "%02X ", data[i]);
      if (n > 0) hex[n * 3 - 1] = '\0';
      ESP_LOGD("redarc_common", "RX id=0x%08X dgn=0x%05X sa=0x%02X len=%u data=[%s]",
               (unsigned) rvc_id, (unsigned) dgn, sa, (unsigned) data.size(), hex);
    }
#endif
    for (auto &cb : this->listeners_) cb(can_id, data);
  }
 private:
  canbus::Canbus *canbus_{nullptr};
  std::vector<std::function<void(uint32_t, const std::vector<uint8_t> &)>> listeners_;
};

class RedarcCommonComponent : public Component {
 public:
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void setup() override { RedarcCanDispatcher::instance().set_canbus(this->canbus_); }
  float get_setup_priority() const override { return setup_priority::BUS; }
 protected:
  canbus::Canbus *canbus_{nullptr};
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
