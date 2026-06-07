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
  this->temp_tank_id_     = 0x1BFD0200UL | sa;
  this->output_status_id_ = 0x1BFD0000UL | sa;
  this->channel_status_id_= 0x1BFCF000UL | sa;
  this->input_status_id_  = 0x13F10800UL | sa;
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void TVMS1280Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS1280 SA 0x%02X", this->source_address_);
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
  if (channel >= 0x04 && channel <= 0x0D) {
    this->publish_output_(channel - 0x04, state);
  } else if (channel == 0x0E && this->inverter_switch_ != nullptr) {
    this->inverter_switch_->publish_state(state);
  }
}


static bool tvms1280_valid_byte_voltage_(uint8_t value) {
  return value != 0xF8 && value != 0xFF;
}

void TVMS1280Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id &= 0x1FFFFFFFUL;
  if (data.size() < 8) return;

  const uint32_t now = millis();
  const bool sensor_ok = (now - this->last_sensor_publish_ms_) >= this->filter_interval_ms_;

  if (can_id == this->input_status_id_) {
    if (sensor_ok && this->supply_voltage_sensor_ != nullptr) {
      this->last_sensor_publish_ms_ = now;
      // TVMS1280 module input/status frame. Logs show D1-D2 decode as
      // little-endian centivolts, e.g. F0 04 -> 0x04F0 / 100 = 12.64 V.
      const uint16_t raw = (uint16_t) data[0] | ((uint16_t) data[1] << 8);
      this->supply_voltage_sensor_->publish_state((float) raw / 100.0f);
    }
    return;
  }

  if (can_id == this->temp_tank_id_) {
    if (sensor_ok) {
      this->last_sensor_publish_ms_ = now;
      if (data[0] == 0x14) {
        if (this->temp1_sensor_ != nullptr) this->temp1_sensor_->publish_state((float) data[1] - 100.0f);
        if (this->voltage_input2_sensor_ != nullptr && tvms1280_valid_byte_voltage_(data[1]))
          this->voltage_input2_sensor_->publish_state((float) data[1] / 10.0f);
        if (this->tank_sensors_[1] != nullptr) this->tank_sensors_[1]->publish_state((float) data[3]);
        if (this->tank_sensors_[2] != nullptr) this->tank_sensors_[2]->publish_state((float) data[5]);
      } else if (data[0] == 0x11) {
        if (this->voltage_input1_sensor_ != nullptr && tvms1280_valid_byte_voltage_(data[1]))
          this->voltage_input1_sensor_->publish_state((float) data[1] / 10.0f);
        if (this->temp2_sensor_ != nullptr) this->temp2_sensor_->publish_state((float) data[5] - 100.0f);
      } else if (data[0] == 0x17) {
        if (this->tank_sensors_[3] != nullptr) this->tank_sensors_[3]->publish_state((float) data[1]);
        if (this->tank_sensors_[4] != nullptr) this->tank_sensors_[4]->publish_state((float) data[3]);
        // DBC v50 confirmed: TVMS1280_Tank5_Percent is MUX 0x17, D6, raw percent.
        // Earlier experimental builds used x1.25; remove that now-confirmed wrong scale.
        if (this->tank_sensors_[5] != nullptr) this->tank_sensors_[5]->publish_state((float) data[5]);
      } else if (data[0] == 0x1A) {
        if (this->tank_sensors_[6] != nullptr) this->tank_sensors_[6]->publish_state((float) data[1]);
      }
    }
    return;
  }

  if (can_id == this->command_id_) {
    if (data[0] == 0xCB && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      const bool state = data[4] != 0;
      // Throttle diagnostic sensors; always update switch state.
      if (sensor_ok) {
        this->last_sensor_publish_ms_ = now;
        if (this->last_command_channel_sensor_ != nullptr) this->last_command_channel_sensor_->publish_state((float) channel);
        if (this->last_command_state_sensor_ != nullptr) this->last_command_state_sensor_->publish_state(state ? 1.0f : 0.0f);
      }
      this->publish_channel_(channel, state);
    }
    return;
  }

  if (can_id == this->output_status_id_) {
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

  if (can_id == this->channel_status_id_) {
    if (data[1] == 0x0E && this->inverter_switch_ != nullptr) this->inverter_switch_->publish_state(data[0] != 0);
    return;
  }
}

}  // namespace redarc_tvms_1280
}  // namespace esphome
