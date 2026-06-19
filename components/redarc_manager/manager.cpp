#include "manager.h"

namespace esphome {
namespace redarc_manager {

static const char *const TAG = "redarc_manager";

void Manager30Component::setup() {
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void Manager30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Manager30 SA 0x%02X", this->source_address_);
  LOG_SENSOR("  ", "Vehicle Input Trigger", this->vehicle_input_trigger_sensor_);
}

void Manager30Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (can_id == 0x0F0001FAUL && data[0] == 0x68 && data[2] == 0x03) {
    if (this->vehicle_input_trigger_sensor_ != nullptr)
      this->vehicle_input_trigger_sensor_->publish_state((float) redarc_common::u16_le(data, 4));
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F206UL, this->source_address_)) {
    if (now - this->last_charger_status_ms_ < this->filter_interval_ms_) return;
    this->last_charger_status_ms_ = now;
    if (this->vehicle_input_trigger_sensor_ != nullptr && data[7] != 0xFF)
      this->vehicle_input_trigger_sensor_->publish_state((float) data[7]);
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F20AUL, this->source_address_)) {
    if (now - this->last_current_ms_ < this->filter_interval_ms_) return;
    this->last_current_ms_ = now;
    const uint32_t raw = redarc_common::u32_le(data, 0);
    const float amps = redarc_common::current_32_centered(raw);
    if (this->output_current_sensor_ != nullptr) this->output_current_sensor_->publish_state(amps);
    if (this->output_current_raw_sensor_ != nullptr) this->output_current_raw_sensor_->publish_state((float) raw);
    if (this->battery_voltage_sensor_ != nullptr) this->battery_voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4) * 0.001f);
    if (this->source_device_current_sensor_ != nullptr && this->battery_sensor_ != nullptr && this->battery_sensor_->has_current()) {
      this->source_device_current_sensor_->publish_state(amps - this->battery_sensor_->current_a());
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F208UL, this->source_address_)) {
    if (now - this->last_solar_ms_ < this->filter_interval_ms_) return;
    this->last_solar_ms_ = now;
    const uint32_t raw_current = redarc_common::u32_le(data, 0);
    const uint16_t raw_voltage = redarc_common::u16_le(data, 4);
    const float solar_a = redarc_common::current_32_centered(raw_current);
    const float solar_v = (float) raw_voltage * 0.001f;
    if (this->solar_current_sensor_ != nullptr) this->solar_current_sensor_->publish_state(solar_a);
    if (this->solar_voltage_sensor_ != nullptr) this->solar_voltage_sensor_->publish_state(solar_v);
    if (this->solar_power_sensor_ != nullptr) this->solar_power_sensor_->publish_state(solar_a * solar_v);
    return;
  }

  // Manager30 solar energy/yield counter. Logs show 0x03FCD601 with D1=0x00
  // increments D2-D5 little-endian in Wh; example: 14 -> 19 Wh while solar
  // power was about 87 W. D1=0x01/0x02 companion frames were observed but
  // remain unknown and are ignored.
  if (redarc_common::rvc_matches(can_id, 0x1FCD6UL, this->source_address_)) {
    if (now - this->last_solar_energy_ms_ < this->filter_interval_ms_) return;
    this->last_solar_energy_ms_ = now;
    if (this->solar_energy_sensor_ != nullptr && data[0] == 0x00) {
      const uint32_t wh = redarc_common::u32_le(data, 1);
      this->solar_energy_sensor_->publish_state((float) wh);
    }
    return;
  }

  // DBC confirmed PGN_03F204_Manager30_AC_DC_Voltage_Candidates:
  // AC_Input_Voltage is D5-D6 little-endian, scale 1 V/count.
  if (redarc_common::rvc_matches(can_id, 0x1F204UL, this->source_address_)) {
    if (now - this->last_ac_ms_ < this->filter_interval_ms_) return;
    this->last_ac_ms_ = now;
    if (this->ac_input_voltage_sensor_ != nullptr) {
      this->ac_input_voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4));
    }
    return;
  }
}

}  // namespace redarc_manager
}  // namespace esphome
