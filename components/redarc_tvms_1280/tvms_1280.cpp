#include "tvms_1280.h"

namespace esphome {
namespace redarc_tvms_1280 {

static const char *const TAG = "redarc_tvms_1280";

namespace {
static constexpr uint8_t TVMS1280_ITEM_INPUT_1 = 0x01;
static constexpr uint8_t TVMS1280_ITEM_INPUT_3 = 0x03;
static constexpr uint8_t TVMS1280_ITEM_OUTPUT_1 = 0x04;
static constexpr uint8_t TVMS1280_ITEM_OUTPUT_10 = 0x0D;
static constexpr uint8_t TVMS1280_ITEM_INVERTER = 0x0E;
static constexpr uint8_t TVMS1280_ITEM_MASTER = 0x0F;
static constexpr uint8_t TVMS1280_ITEM_VOLTAGE_INPUT_1 = 0x11;
static constexpr uint8_t TVMS1280_ITEM_TEMP_2 = 0x14;
static constexpr uint8_t TVMS1280_ITEM_TANK_3 = 0x17;
static constexpr uint8_t TVMS1280_ITEM_TANK_6 = 0x1A;
}  // namespace

void TVMS1280Switch::write_state(bool state) {
  if (this->parent_ != nullptr) this->parent_->send_channel(this->channel_, state);
  this->publish_state(state);
}

void TVMS1280Component::setup() {
  const uint8_t sa = this->source_address_;
  const uint8_t ha = this->host_address_;
  this->command_id_       = 0x0F000000UL | ((uint32_t) sa << 8) | ha;
  this->output_state_.fill(0xFF);  // 0xFF = unseen, so the first frame publishes
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void TVMS1280Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS1280 SA 0x%02X", this->source_address_);
  LOG_BINARY_SENSOR("  ", "Digital Input 1", this->digital_input_sensors_[1]);
  LOG_BINARY_SENSOR("  ", "Digital Input 2", this->digital_input_sensors_[2]);
  LOG_BINARY_SENSOR("  ", "Digital Input 3", this->digital_input_sensors_[3]);
  LOG_TEXT_SENSOR("  ", "Output Status", this->output_status_text_sensor_);
}

void TVMS1280Component::register_output_switch(TVMS1280Switch *sw) {
  if (sw == nullptr) return;
  const uint8_t n = sw->output_number();
  if (n <= 9) this->output_switches_[n] = sw;
}

void TVMS1280Component::send_channel(uint8_t channel, bool state) {
  const std::vector<uint8_t> data = {0xCB, 0x00, 0xFF, channel, (uint8_t) (state ? 0x01 : 0x00), 0x00, 0x00, 0x00};
  redarc_common::send_command(this->command_id_, data);
}

void TVMS1280Component::publish_output_(uint8_t output_number, bool state) {
  if (output_number > 9) return;
  if (this->output_switches_[output_number] != nullptr) this->output_switches_[output_number]->publish_state(state);
}

void TVMS1280Component::publish_output_status_value_(uint8_t idx, uint8_t value) {
  if (idx >= this->output_status_sensors_.size()) return;
  sensor::Sensor *s = this->output_status_sensors_[idx];
  if (s == nullptr) return;
  // 0xF8 unconfigured / 0xFF no-data -> NaN, which Home Assistant shows as
  // unavailable. Otherwise publish the raw status code (0 off, 1 on, 6 fuse
  // blown, 10 over temp, 20 off override, 21 on override).
  if (value == 0xF8 || value == 0xFF) {
    s->publish_state(NAN);
  } else {
    s->publish_state((float) value);
  }
}

void TVMS1280Component::publish_output_status_() {
  if (this->output_status_text_sensor_ == nullptr) return;
  std::string summary;
  for (uint8_t i = 0; i < this->output_state_.size(); i++) {
    const char *st = redarc_common::output_status_name(this->output_state_[i]);
    if (st == nullptr) continue;  // 0x00/0x01/0xFF: normal or no-data
    if (!summary.empty()) summary += ", ";
    summary += "Output " + std::to_string(i) + " " + st;
  }
  if (summary.empty()) summary = "None";
  if (summary == this->last_output_status_) return;
  this->last_output_status_ = summary;
  this->output_status_text_sensor_->publish_state(summary);
}

void TVMS1280Component::publish_channel_(uint8_t channel, bool state) {
  if (channel >= TVMS1280_ITEM_INPUT_1 && channel <= TVMS1280_ITEM_INPUT_3) {
    if (this->digital_input_sensors_[channel] != nullptr) this->digital_input_sensors_[channel]->publish_state(state);
  } else if (channel >= TVMS1280_ITEM_OUTPUT_1 && channel <= TVMS1280_ITEM_OUTPUT_10) {
    this->publish_output_(channel - TVMS1280_ITEM_OUTPUT_1, state);
  } else if (channel == TVMS1280_ITEM_INVERTER && this->inverter_switch_ != nullptr) {
    this->inverter_switch_->publish_state(state);
  } else if (channel == TVMS1280_ITEM_MASTER && this->master_switch_ != nullptr) {
    this->master_switch_->publish_state(state);
  }
}


void TVMS1280Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (redarc_common::rvc_matches(can_id, 0x1FD02UL, this->source_address_)) {
    if (data[0] == TVMS1280_ITEM_VOLTAGE_INPUT_1) {
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
    } else if (data[0] == TVMS1280_ITEM_TEMP_2) {
      if (now - this->last_mux_0x14_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x14_ms_ = now;
      if (this->temp1_sensor_ != nullptr) this->temp1_sensor_->publish_state((float) data[1] - 100.0f);
      if (this->tank_sensors_[1] != nullptr) this->tank_sensors_[1]->publish_state((float) data[3]);
      if (this->tank_sensors_[2] != nullptr) this->tank_sensors_[2]->publish_state((float) data[5]);
    } else if (data[0] == TVMS1280_ITEM_TANK_3) {
      if (now - this->last_mux_0x17_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x17_ms_ = now;
      if (this->tank_sensors_[3] != nullptr) this->tank_sensors_[3]->publish_state((float) data[1]);
      if (this->tank_sensors_[4] != nullptr) this->tank_sensors_[4]->publish_state((float) data[3]);
      if (this->tank_sensors_[5] != nullptr) this->tank_sensors_[5]->publish_state((float) data[5]);
    } else if (data[0] == TVMS1280_ITEM_TANK_6) {
      if (now - this->last_mux_0x1A_ms_ < this->filter_interval_ms_) return;
      this->last_mux_0x1A_ms_ = now;
      if (this->tank_sensors_[6] != nullptr) this->tank_sensors_[6]->publish_state((float) data[1]);
    } else if (now - this->last_unknown_mux_ms_ >= this->filter_interval_ms_) {
      this->last_unknown_mux_ms_ = now;
      ESP_LOGV(TAG, "TVMS1280 unknown 0x1FD02 item page 0x%02X: %02X %02X %02X %02X %02X %02X %02X %02X",
               data[0], data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
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
    // TVMS1280 output feedback. D1 is the base channel and D2-D8 are the states
    // for base+0..base+6. Status codes: 0x00 off, 0x01 on, 0x06 fuse blown,
    // 0x0A output over temp, 0x14 off override, 0x15 on override, 0xF8
    // unconfigured, 0xFF no-data. Off/On and the override variants drive the
    // switch state; all non-normal codes (incl. 0xF8 unconfigured) are
    // summarised in the Output Status text sensor. (ESPHome cannot mark an
    // individual switch entity unavailable, so 0xF8 is reported, not disabled.)
    const uint8_t base_channel = data[0];
    bool output_seen = false;
    for (uint8_t i = 1; i < 8; i++) {
      const uint8_t value = data[i];
      const uint8_t channel = base_channel + i - 1;
      if (channel >= TVMS1280_ITEM_OUTPUT_1 && channel <= TVMS1280_ITEM_OUTPUT_10) {
        const uint8_t idx = channel - TVMS1280_ITEM_OUTPUT_1;
        if (value != this->output_state_[idx]) {
          this->output_state_[idx] = value;
          this->publish_output_status_value_(idx, value);
        }
        output_seen = true;
      }
      switch (value) {
        case 0x00:  // Off
        case 0x14:  // Off override (state still updates)
          this->publish_channel_(channel, false);
          break;
        case 0x01:  // On
        case 0x15:  // On override (state still updates)
          this->publish_channel_(channel, true);
          break;
        default:    // 0x06 fuse blown, 0x0A over temp, 0xF8 unconfigured, 0xFF
          break;    // no-data: leave switch state; reported via Output Status

      }
    }
    if (output_seen) this->publish_output_status_();
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FCF0UL, this->source_address_)) {
    if (data[1] == TVMS1280_ITEM_INVERTER && this->inverter_switch_ != nullptr) this->inverter_switch_->publish_state(data[0] != 0);
    return;
  }
}

}  // namespace redarc_tvms_1280
}  // namespace esphome
