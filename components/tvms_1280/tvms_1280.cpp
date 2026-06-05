#include "tvms_1280.h"

namespace esphome {
namespace tvms_1280 {

static const char *const TAG = "tvms_1280";

void TVMS1280Switch::write_state(bool state) {
  if (this->parent_ != nullptr) this->parent_->send_channel(this->channel_, state);
  this->publish_state(state);
}

void TVMS1280Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS1280 SA 0x%02X", this->source_address_);
}

void TVMS1280Component::register_output_switch(TVMS1280Switch *sw) {
  if (sw == nullptr) return;
  const uint8_t n = sw->output_number();
  if (n >= 1 && n <= 10) this->output_switches_[n] = sw;
}

void TVMS1280Component::send_channel(uint8_t channel, bool state) {
  if (this->canbus_ == nullptr) return;
  this->canbus_->send_data(this->command_id_, true, {0xCB, 0x00, 0xFF, channel, (uint8_t) (state ? 0x01 : 0x00), 0x00, 0x00, 0x00});
  if (this->last_command_channel_sensor_ != nullptr) this->last_command_channel_sensor_->publish_state((float) channel);
  if (this->last_command_state_sensor_ != nullptr) this->last_command_state_sensor_->publish_state(state ? 1.0f : 0.0f);
}

void TVMS1280Component::publish_output_(uint8_t output_number, bool state) {
  if (output_number < 1 || output_number > 10) return;
  if (this->output_switches_[output_number] != nullptr) this->output_switches_[output_number]->publish_state(state);
}

void TVMS1280Component::publish_channel_(uint8_t channel, bool state) {
  if (channel >= 0x04 && channel <= 0x0D) {
    this->publish_output_(channel - 0x03, state);
  } else if (channel == 0x0E && this->inverter_switch_ != nullptr) {
    this->inverter_switch_->publish_state(state);
  }
}

void TVMS1280Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id &= 0x1FFFFFFFUL;
  if (data.size() < 8) return;

  if (can_id == this->temp_tank_id_) {
    if (data[0] == 0x14) {
      if (this->temp1_sensor_ != nullptr) this->temp1_sensor_->publish_state((float) data[1] - 100.0f);
      if (this->tank_sensors_[1] != nullptr) this->tank_sensors_[1]->publish_state((float) data[3]);
      if (this->tank_sensors_[2] != nullptr) this->tank_sensors_[2]->publish_state((float) data[5]);
    } else if (data[0] == 0x11) {
      if (this->temp2_sensor_ != nullptr) this->temp2_sensor_->publish_state((float) data[5] - 100.0f);
    } else if (data[0] == 0x17) {
      if (this->tank_sensors_[3] != nullptr) this->tank_sensors_[3]->publish_state((float) data[1]);
      if (this->tank_sensors_[4] != nullptr) this->tank_sensors_[4]->publish_state((float) data[3]);
      if (this->tank_sensors_[5] != nullptr) this->tank_sensors_[5]->publish_state((float) data[5] * 1.25f);
    } else if (data[0] == 0x1A) {
      if (this->tank_sensors_[6] != nullptr) this->tank_sensors_[6]->publish_state((float) data[1]);
    }
    return;
  }

  if (can_id == this->command_id_) {
    if (data[0] == 0xCB && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      const bool state = data[4] != 0;
      if (this->last_command_channel_sensor_ != nullptr) this->last_command_channel_sensor_->publish_state((float) channel);
      if (this->last_command_state_sensor_ != nullptr) this->last_command_state_sensor_->publish_state(state ? 1.0f : 0.0f);
      this->publish_channel_(channel, state);
    }
    return;
  }

  if (can_id == this->output_status_id_) {
    if (data[0] == 0x08) {
      // Confirmed by logs/user: Output 6 = channel 0x09, Output 7 = channel 0x0A, inverter bit appears at D8 bit0.
      this->publish_output_(6, (data[2] & 0x01) != 0);
      this->publish_output_(7, (data[3] & 0x01) != 0);
      if (this->inverter_switch_ != nullptr) this->inverter_switch_->publish_state((data[7] & 0x01) != 0);
    }
    return;
  }

  if (can_id == this->channel_status_id_) {
    if (data[1] == 0x0E && this->inverter_switch_ != nullptr) this->inverter_switch_->publish_state(data[0] != 0);
    return;
  }
}

}  // namespace tvms_1280
}  // namespace esphome
