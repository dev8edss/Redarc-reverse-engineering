#include "manager30.h"

namespace esphome {
namespace manager30 {

static const char *const TAG = "manager30";

void Manager30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Manager30 SA 0x%02X", this->source_address_);
}

void Manager30Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id &= 0x1FFFFFFFUL;
  if (data.size() < 8) return;

  const uint32_t id_current = redarc_common::with_sa(0x03F20A00UL, this->source_address_);
  const uint32_t id_solar = redarc_common::with_sa(0x03F20800UL, this->source_address_);
  // Manager30 solar energy/yield counter. Logs show 0x03FCD601 with D1=0x00
  // increments D2-D5 little-endian in Wh; example: 14 -> 19 Wh while solar
  // power was about 87 W. D1=0x01/0x02 companion frames were observed but
  // remain unknown and are ignored.
  const uint32_t id_solar_energy = redarc_common::with_sa(0x03FCD600UL, this->source_address_);
  // DBC confirmed PGN_03F204_Manager30_AC_DC_Voltage_Candidates:
  // AC_Input_Voltage is raw CAN ID 0x03F20401, D5-D6 little-endian,
  // scale 1 V/count. D7-D8 DC_Input_Voltage_Raw remains unconfirmed and is
  // deliberately not exposed as a normal sensor yet.
  const uint32_t id_ac = redarc_common::with_sa(0x03F20400UL, this->source_address_);

  if (can_id == id_current) {
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

  if (can_id == id_solar) {
    const uint32_t raw_current = redarc_common::u32_le(data, 0);
    const uint16_t raw_voltage = redarc_common::u16_le(data, 4);
    const float solar_a = redarc_common::current_32_centered(raw_current);
    const float solar_v = (float) raw_voltage * 0.001f;
    if (this->solar_current_sensor_ != nullptr) this->solar_current_sensor_->publish_state(solar_a);
    if (this->solar_voltage_sensor_ != nullptr) this->solar_voltage_sensor_->publish_state(solar_v);
    if (this->solar_power_sensor_ != nullptr) this->solar_power_sensor_->publish_state(solar_a * solar_v);
    return;
  }

  if (can_id == id_solar_energy) {
    if (this->solar_energy_sensor_ != nullptr && data[0] == 0x00) {
      const uint32_t wh = redarc_common::u32_le(data, 1);
      this->solar_energy_sensor_->publish_state((float) wh);
    }
    return;
  }

  if (can_id == id_ac) {
    if (this->ac_input_voltage_sensor_ != nullptr) {
      const uint16_t ac_v = redarc_common::u16_le(data, 4);
      this->ac_input_voltage_sensor_->publish_state((float) ac_v);
    }
    return;
  }
}

}  // namespace manager30
}  // namespace esphome
