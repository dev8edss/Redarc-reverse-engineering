#include "tvms_1280.h"

namespace esphome {
namespace redarc_tvms_1280 {

static const char *const TAG = "redarc_tvms_1280";

void TVMS1280Switch::write_state(bool state) {
  if (this->parent_ != nullptr) this->parent_->send_channel(this->channel_, state);
  this->publish_state(state);
}

void TVMS1280Component::setup() {
  const uint8_t sa = this->source_address_;
  const uint8_t ha = this->host_address_;
  this->command_id_       = 0x0F000000UL | ((uint32_t) sa << 8) | ha;
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void TVMS1280Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS1280 SA 0x%02X", this->source_address_);
  LOG_BINARY_SENSOR("  ", "Digital Input 1", this->digital_input_sensors_[1]);
  LOG_BINARY_SENSOR("  ", "Digital Input 2", this->digital_input_sensors_[2]);
  LOG_BINARY_SENSOR("  ", "Digital Input 3", this->digital_input_sensors_[3]);
}

void TVMS1280Component::register_output_switch(TVMS1280Switch *sw) {
  if (sw == nullptr) return;
  const uint8_t n = sw->output_number();
  if (n <= 9) this->output_switches_[n] = sw;
}

void TVMS1280Component::send_channel(uint8_t channel, bool state) {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  bus->send_data(this->command_id_, true, {0xCB, 0x00, 0xFF, channel, (uint8_t) (state ? 0x01 : 0x00), 0x00, 0x00, 0x00});
  if (this->last_command_channel_sensor_ != nullptr) this->last_command_channel_sensor_->publish_state((float) channel);
  if (this->last_command_state_sensor_ != nullptr) this->last_command_state_sensor_->publish_state(state ? 1.0f : 0.0f);
}

void TVMS1280Component::publish_output_(uint8_t output_number, bool state) {
  if (output_number > 9) return;
  if (this->output_switches_[output_number] != nullptr) this->output_switches_[output_number]->publish_state(state);
}

void TVMS1280Component::publish_channel_(uint8_t channel, bool state) {
  if (channel >= 0x01 && channel <= 0x03) {
    if (this->digital_input_sensors_[channel] != nullptr) this->digital_input_sensors_[channel]->publish_state(state);
  } else if (channel >= 0x04 && channel <= 0x0D) {
    this->publish_output_(channel - 0x04, state);
  } else if (channel == 0x0E && this->inverter_switch_ != nullptr) {
    this->inverter_switch_->publish_state(state);
  } else if (channel == 0x0F && this->master_switch_ != nullptr) {
    this->master_switch_->publish_state(state);
  }
}


void TVMS1280Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (redarc_common::rvc_matches(can_id, 0x1F108UL, this->source_address_)) {
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FD02UL, this->source_address_)) {
    if (data[0] == 0x11) {
      if (now - this->last_mux_0x11_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x11_ms_ = now;
      if (this->voltage_input1_sensor_ != nullptr) {
        const uint16_t raw_mv1 = (uint16_t) data[1] | ((uint16_t) data[2] << 8);
        this->voltage_input1_sensor_->publish_state((float) raw_mv1 / 1000.0f);
      }
      if (this->voltage_input2_sensor_ != nullptr) {
        const uint16_t raw_mv2 = (uint16_t) data[3] | ((uint16_t) data[4] << 8);
        this->voltage_input2_sensor_->publish_state((float) raw_mv2 / 1000.0f);
      }
      if (this->temp2_sensor_ != nullptr) this->temp2_sensor_->publish_state((float) data[5] - 100.0f);
    } else if (data[0] == 0x14) {
      if (now - this->last_mux_0x14_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x14_ms_ = now;
      if (this->temp1_sensor_ != nullptr) this->temp1_sensor_->publish_state((float) data[1] - 100.0f);
      if (this->tank_sensors_[1] != nullptr) this->tank_sensors_[1]->publish_state((float) data[3]);
      if (this->tank_sensors_[2] != nullptr) this->tank_sensors_[2]->publish_state((float) data[5]);
    } else if (data[0] == 0x17) {
      if (now - this->last_mux_0x17_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x17_ms_ = now;
      if (this->tank_sensors_[3] != nullptr) this->tank_sensors_[3]->publish_state((float) data[1]);
      if (this->tank_sensors_[4] != nullptr) this->tank_sensors_[4]->publish_state((float) data[3]);
      // DBC v50 confirmed: TVMS1280_Tank5_Percent is MUX 0x17, D6, raw percent.
      // Earlier experimental builds used x1.25; remove that now-confirmed wrong scale.
      if (this->tank_sensors_[5] != nullptr) this->tank_sensors_[5]->publish_state((float) data[5]);
    } else if (data[0] == 0x1A) {
      if (now - this->last_mux_0x1A_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x1A_ms_ = now;
      if (this->tank_sensors_[6] != nullptr) this->tank_sensors_[6]->publish_state((float) data[1]);
    }
    return;
  }

  if (can_id == this->command_id_) {
    if (data[0] == 0xCB && data[2] == 0xFF) {
      this->publish_channel_(data[3], data[4] != 0);
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FD00UL, this->source_address_)) {
    // TVMS1280 output feedback. Logs show D1 is the base channel and D2-D8
    // contain the states for base+0 through base+6.
    // Valid states are 0x00 = OFF and 0x01 = ON. Values such as 0xF8/0xFF
    // are unavailable/no-data markers and must not overwrite HA state.
    const uint8_t base_channel = data[0];
    for (uint8_t i = 1; i < 8; i++) {
      const uint8_t value = data[i];
      if (value != 0x00 && value != 0x01) continue;
      const uint8_t channel = base_channel + i - 1;
      this->publish_channel_(channel, value == 0x01);
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FCF0UL, this->source_address_)) {
    if (data[1] == 0x0E && this->inverter_switch_ != nullptr) this->inverter_switch_->publish_state(data[0] != 0);
    return;
  }
}

}  // namespace redarc_tvms_1280
}  // namespace esphome
