#include "battery_sensor.h"

namespace esphome {
namespace redarc_battery_sensor {

static const char *const TAG = "redarc_battery_sensor";

void BatterySensorComponent::setup() {
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void BatterySensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Battery Sensor SA 0x%02X", this->source_address_);
}

void BatterySensorComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id &= 0x1FFFFFFFUL;
  if (data.size() < 8) return;

  const uint32_t id_current = redarc_common::with_sa(0x13F10200UL, this->source_address_);
  const uint32_t id_soc = redarc_common::with_sa(0x13F10400UL, this->source_address_);

  const uint32_t now = millis();
  const bool publish_ok = (now - this->last_publish_ms_) >= this->filter_interval_ms_;

  if (can_id == id_current) {
    const uint32_t raw = redarc_common::u32_le(data, 0);
    const float amps = redarc_common::current_32_centered(raw);
    this->current_a_ = amps;  // always update for manager's reference
    if (publish_ok) {
      this->last_publish_ms_ = now;
      if (this->current_sensor_ != nullptr) this->current_sensor_->publish_state(amps);
      if (this->current_raw_sensor_ != nullptr) this->current_raw_sensor_->publish_state((float) raw);
      if (this->voltage_sensor_ != nullptr) this->voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4) * 0.001f);
      if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state((float) data[6] - 60.0f);
    }
    return;
  }

  if (can_id == id_soc) {
    if (publish_ok) {
      this->last_publish_ms_ = now;
      if (this->soc_sensor_ != nullptr) this->soc_sensor_->publish_state((float) data[0]);
    }
    return;
  }
}

}  // namespace redarc_battery_sensor
}  // namespace esphome
