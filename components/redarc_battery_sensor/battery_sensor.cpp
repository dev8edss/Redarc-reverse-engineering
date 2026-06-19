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
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  if (redarc_common::rvc_matches(can_id, 0x1F102UL, this->source_address_)) {
    const uint32_t raw = redarc_common::u32_le(data, 0);
    const float amps = redarc_common::current_32_centered(raw);
    this->current_a_ = amps;
    const uint32_t now = millis();
    if (now - this->last_current_ms_ >= this->filter_interval_ms_) {
      this->last_current_ms_ = now;
      if (this->current_sensor_ != nullptr) this->current_sensor_->publish_state(amps);
      if (this->current_raw_sensor_ != nullptr) this->current_raw_sensor_->publish_state((float) raw);
      if (this->voltage_sensor_ != nullptr) this->voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4) * 0.001f);
      if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state((float) data[6] - 60.0f);
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F104UL, this->source_address_)) {
    const uint32_t now = millis();
    if (now - this->last_soc_ms_ >= this->filter_interval_ms_) {
      this->last_soc_ms_ = now;
      if (this->soc_sensor_ != nullptr) this->soc_sensor_->publish_state((float) data[0]);
    }
    return;
  }
}

}  // namespace redarc_battery_sensor
}  // namespace esphome
