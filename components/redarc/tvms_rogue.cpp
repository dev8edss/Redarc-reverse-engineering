#include "tvms_rogue.h"

namespace esphome {
namespace redarc_tvms_rogue {

static const char *const TAG = "redarc_tvms_rogue";

namespace {
static constexpr uint8_t ROGUE_ITEM_DIGITAL_INPUT_1 = 0x01;
static constexpr uint8_t ROGUE_ITEM_DIGITAL_INPUT_8 = 0x08;
static constexpr uint8_t ROGUE_ITEM_TANK_1 = 0x09;
static constexpr uint8_t ROGUE_ITEM_MASTER = 0x0B;
static constexpr uint8_t ROGUE_ITEM_OUTPUT_1 = 0x0C;
static constexpr uint8_t ROGUE_ITEM_OUTPUT_10 = 0x15;
static constexpr uint8_t ROGUE_ITEM_INPUT_VOLTAGE = 0x16;
}  // namespace

void TVMSRogueSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) this->parent_->send_master(state);
  this->publish_state(state);
}

light::LightTraits TVMSRogueLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void TVMSRogueLight::setup_state(light::LightState *state) {
  this->state_ = state;
}

void TVMSRogueLight::publish_feedback_level(float level_percent) {
  if (this->state_ == nullptr) return;

  if (level_percent < 0.0f) level_percent = 0.0f;
  if (level_percent > 100.0f) level_percent = 100.0f;

  const bool on = level_percent > 0.5f;
  this->last_sent_percent_ = on ? (uint8_t) std::round(level_percent) : 0;

  // Feedback for each intermediate CAN level must not replace ESPHome's final
  // remote target while a transition is running, otherwise the transformer stops
  // at the first reported step. The separate level sensor still receives every
  // physical feedback update through TVMSRogueComponent::set_feedback_level_().
  if (this->state_->is_transformer_active()) return;

  if (on) this->last_nonzero_level_percent_ = level_percent;
  const float retained_brightness = this->last_nonzero_level_percent_ / 100.0f;

  // Preserve the last non-zero brightness while OFF. This matches normal light
  // behaviour and lets a plain turn_on transition back to the previous level.
  this->state_->current_values.set_state(on);
  this->state_->current_values.set_brightness(on ? level_percent / 100.0f : retained_brightness);
  this->state_->remote_values.set_state(on);
  this->state_->remote_values.set_brightness(on ? level_percent / 100.0f : retained_brightness);
  this->state_->publish_state();
}

void TVMSRogueLight::write_state(light::LightState *state) {
  if (this->parent_ == nullptr) return;

  // ESPHome updates current_values on every transition step. remote_values is the
  // final requested target and using it here would make every transition instant.
  bool current_on = false;
  float current_brightness = 0.0f;
  state->current_values_as_binary(&current_on);
  state->current_values_as_brightness(&current_brightness);

  const bool target_on = state->remote_values.is_on();
  const float target_brightness = state->remote_values.get_brightness();

  // A plain turn_on can occasionally arrive with an ON state but zero brightness.
  // Restore the last physical non-zero level instead of translating it to OFF.
  if (target_on && target_brightness <= 0.0f) {
    uint8_t percent = (uint8_t) std::round(this->last_nonzero_level_percent_);
    if (percent < 1) percent = 100;
    if (percent > 100) percent = 100;
    if (percent == this->last_sent_percent_) return;
    this->last_sent_percent_ = percent;
    this->parent_->set_target(this->output_number_, this->channel_, (float) percent);
    return;
  }

  float current_percent = current_on ? current_brightness * 100.0f : 0.0f;

  // During fade-out, continue sending interpolated levels until the true-off
  // threshold is reached, then send one real OFF command.
  if (!target_on && (!current_on || current_percent <= this->parent_->true_off_threshold())) {
    if (this->last_sent_percent_ == 0) return;
    this->last_sent_percent_ = 0;
    this->parent_->turn_off(this->output_number_, this->channel_);
    return;
  }

  // During fade-in the Rogue remains physically off until the first useful level.
  if (target_on && current_percent <= this->parent_->true_off_threshold()) return;

  if (current_percent > 100.0f) current_percent = 100.0f;
  uint8_t percent = (uint8_t) std::round(current_percent);
  if (percent < 1) percent = 1;
  if (percent > 100) percent = 100;
  if (percent == this->last_sent_percent_) return;

  this->last_sent_percent_ = percent;
  if (target_on) this->last_nonzero_level_percent_ = current_percent;
  this->parent_->set_target(this->output_number_, this->channel_, (float) percent);
}

void TVMSRogueComponent::setup() {
  for (auto &level : this->levels_) level = NAN;
  const uint8_t sa = this->source_address_;
  const uint8_t ha = this->host_address_;
  this->output_command_id_ = 0x0F000000UL | ((uint32_t) sa << 8) | ha;
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void TVMSRogueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS Rogue SA=0x%02X HA=0x%02X", this->source_address_, this->host_address_);
  ESP_LOGCONFIG(TAG, "  Output cmd: 0x%08X  DGN: 0x1FD00, 0x1FD02, 0x1FD12", this->output_command_id_);
  LOG_SENSOR("  ", "Input Voltage", this->input_voltage_sensor_);
  LOG_SENSOR("  ", "Input Current", this->input_current_sensor_);
  LOG_TEXT_SENSOR("  ", "Output Status", this->output_status_text_sensor_);
  LOG_SWITCH("  ", "Master", this->master_switch_);
  ESP_LOGCONFIG(TAG, "  True-off threshold: %.1f%%", this->true_off_threshold_percent_);
}

void TVMSRogueComponent::register_light(TVMSRogueLight *light) {
  if (light == nullptr) return;
  uint8_t out = light->output_number();
  if (out < 1 || out > 10) return;
  this->lights_[out] = light;
}

float TVMSRogueComponent::level(uint8_t output_number) const {
  if (output_number < 1 || output_number > 10) return NAN;
  return this->levels_[output_number];
}

void TVMSRogueComponent::send_frame_(uint32_t id, const std::vector<uint8_t> &data) {
  redarc_common::send_command(id, data);
}

void TVMSRogueComponent::send_on_(uint8_t channel) {
  this->send_frame_(this->output_command_id_, {0xCB, 0x00, 0xFF, channel, 0x01, 0x00, 0x00, 0x00});
}

void TVMSRogueComponent::send_off_(uint8_t channel) {
  this->send_frame_(this->output_command_id_, {0xCB, 0x00, 0xFF, channel, 0x00, 0x00, 0x00, 0x00});
}

void TVMSRogueComponent::send_level_(uint8_t channel, uint8_t percent) {
  if (percent > 100) percent = 100;
  this->send_frame_(this->output_command_id_, {0x5A, 0x01, 0xFF, channel, percent, 0x00, 0x00, 0x00});
}

void TVMSRogueComponent::send_master(bool state) {
  if (state) {
    this->send_on_(ROGUE_ITEM_MASTER);
  } else {
    this->send_off_(ROGUE_ITEM_MASTER);
  }
  ESP_LOGI(TAG, "Master channel 0x0B %s", state ? "ON" : "OFF");
}

void TVMSRogueComponent::turn_off(uint8_t output_number, uint8_t channel) {
  if (output_number < 1 || output_number > 10) return;

  this->send_off_(channel);
  ESP_LOGI(TAG, "Output %u channel 0x%02X OFF", output_number, channel);
}

void TVMSRogueComponent::set_target(uint8_t output_number, uint8_t channel, float target_percent) {
  if (output_number < 1 || output_number > 10) return;
  if (target_percent <= this->true_off_threshold_percent_) {
    this->turn_off(output_number, channel);
    return;
  }
  if (target_percent > 100.0f) target_percent = 100.0f;

  const uint8_t percent = (uint8_t) std::round(target_percent);
  if (percent >= 100) {
    this->send_on_(channel);
    ESP_LOGI(TAG, "Output %u channel 0x%02X ON", output_number, channel);
  } else {
    this->send_level_(channel, percent);
    ESP_LOGI(TAG, "Output %u channel 0x%02X level %u%%", output_number, channel, percent);
  }
}

void TVMSRogueComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (redarc_common::rvc_matches(can_id, 0x1FD02UL, this->source_address_)) {
    if (data[0] == ROGUE_ITEM_TANK_1) {
      if (now - this->last_tank_ms_ >= this->filter_interval_ms_) {
        this->last_tank_ms_ = now;
        if (this->tank1_sensor_ != nullptr) this->tank1_sensor_->publish_state((float) data[1]);
        if (this->tank2_sensor_ != nullptr) this->tank2_sensor_->publish_state((float) data[2]);
      }
    } else if (data[0] == ROGUE_ITEM_INPUT_VOLTAGE) {
      // Input page: D2-D3 voltage, D4-D5 current, both little-endian, raw / 1000.
      if (now - this->last_input_voltage_ms_ >= this->filter_interval_ms_) {
        this->last_input_voltage_ms_ = now;
        if (this->input_voltage_sensor_ != nullptr) {
          const uint16_t raw_mv = (uint16_t) data[1] | ((uint16_t) data[2] << 8);
          this->input_voltage_sensor_->publish_state((float) raw_mv / 1000.0f);
        }
        if (this->input_current_sensor_ != nullptr) {
          const uint16_t raw_ma = (uint16_t) data[3] | ((uint16_t) data[4] << 8);
          this->input_current_sensor_->publish_state((float) raw_ma / 1000.0f);
        }
      }
    } else if (now - this->last_unknown_1fd02_ms_ >= this->filter_interval_ms_) {
      this->last_unknown_1fd02_ms_ = now;
      ESP_LOGV(TAG, "Rogue unknown 0x1FD02 item page 0x%02X: %02X %02X %02X %02X %02X %02X %02X %02X",
               data[0], data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    }
    return;
  }

  if (can_id == this->output_command_id_) {
    // Output command events are not throttled; they reflect requested state changes.
    if (data[0] == 0xCB && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      this->publish_channel_(channel, data[4] != 0);
    } else if (data[0] == 0x5A && data[1] == 0x01 && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      if (channel >= 0x0C && channel <= 0x15 && data[4] <= 100) {
        const uint8_t output_number = channel - 0x0B;
        this->set_feedback_level_(output_number, (float) data[4]);
      }
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FD00UL, this->source_address_)) {
    this->handle_channel_status_frame_(data);
    return;
  }

  if (!redarc_common::rvc_matches(can_id, 0x1FD12UL, this->source_address_)) return;

  if (data[0] == 0x0C) {
    for (uint8_t i = 1; i <= 7; i++) this->set_feedback_level_(i, (float) data[i]);
  } else if (data[0] == 0x13) {
    this->set_feedback_level_(8, (float) data[1]);
    this->set_feedback_level_(9, (float) data[2]);
    this->set_feedback_level_(10, (float) data[3]);
  }
}

void TVMSRogueComponent::publish_channel_(uint8_t channel, bool state) {
  if (channel == ROGUE_ITEM_MASTER) {
    if (this->master_switch_ != nullptr) this->master_switch_->publish_state(state);
    return;
  }
  if (channel < ROGUE_ITEM_OUTPUT_1 || channel > ROGUE_ITEM_OUTPUT_10) return;

  const uint8_t output_number = channel - ROGUE_ITEM_MASTER;
  if (!state) {
    this->set_feedback_level_(output_number, 0.0f);
    return;
  }

  float current = this->level(output_number);
  if (std::isnan(current) || current <= this->true_off_threshold_percent_) current = 100.0f;
  this->set_feedback_level_(output_number, current);
}

void TVMSRogueComponent::handle_channel_status_frame_(const std::vector<uint8_t> &data) {
  if (data.size() < 8) return;

  // DGN 0x1FD00 is a paginated byte-state array. D1 is the base item ID and
  // D2-D8 are states for base..base+6. Status codes: 0x00 off, 0x01 on, 0x06
  // fuse blown, 0x0A over temp, 0x14/0x15 off/on override, 0xF8 unconfigured,
  // 0xFF no-data. Only 0x00/0x01 drive button/master state; non-normal codes
  // are summarised in the Output Status text sensor.
  const uint8_t base_channel = data[0];
  bool output_seen = false;
  for (uint8_t i = 1; i <= 7; i++) {
    const uint8_t value = data[i];
    const uint8_t channel = base_channel + i - 1;
    if (channel >= ROGUE_ITEM_OUTPUT_1 && channel <= ROGUE_ITEM_OUTPUT_10) {
      this->output_state_[channel - ROGUE_ITEM_OUTPUT_1] = value;
      output_seen = true;
    }
    if (value != 0x00 && value != 0x01) continue;
    if (channel >= ROGUE_ITEM_DIGITAL_INPUT_1 && channel <= ROGUE_ITEM_DIGITAL_INPUT_8) {
      this->set_button_state_(channel, value == 0x01);
    } else if (channel == ROGUE_ITEM_MASTER && this->master_switch_ != nullptr) {
      this->master_switch_->publish_state(value == 0x01);
    }
  }
  if (output_seen) this->publish_output_status_();
}

void TVMSRogueComponent::publish_output_status_() {
  if (this->output_status_text_sensor_ == nullptr) return;
  std::string summary;
  for (uint8_t i = 0; i < this->output_state_.size(); i++) {
    const char *st = redarc_common::output_status_name(this->output_state_[i]);
    if (st == nullptr) continue;  // 0x00/0x01/0xFF: normal or no-data
    if (!summary.empty()) summary += ", ";
    summary += "Output " + std::to_string(i + 1) + " " + st;
  }
  if (summary.empty()) summary = "None";
  if (summary == this->last_output_status_) return;
  this->last_output_status_ = summary;
  this->output_status_text_sensor_->publish_state(summary);
}

void TVMSRogueComponent::set_button_state_(uint8_t output_number, bool active) {
  if (output_number < 1 || output_number > 8) return;
  if (this->button_state_known_[output_number] && this->button_states_[output_number] == active) return;

  this->button_state_known_[output_number] = true;
  this->button_states_[output_number] = active;
  if (this->button_sensors_[output_number] != nullptr) {
    this->button_sensors_[output_number]->publish_state(active);
  }
}

void TVMSRogueComponent::set_feedback_level_(uint8_t output_number, float level) {
  if (output_number < 1 || output_number > 10) return;
  if (level < 0.0f) level = 0.0f;
  if (level > 100.0f) level = 100.0f;
  const bool changed = this->levels_[output_number] != level;
  this->levels_[output_number] = level;
  if (changed && this->level_sensors_[output_number] != nullptr)
    this->level_sensors_[output_number]->publish_state(level);
  this->publish_actual_light_level_(output_number);
}

void TVMSRogueComponent::publish_actual_light_level_(uint8_t output_number) {
  if (output_number < 1 || output_number > 10) return;
  if (this->lights_[output_number] == nullptr) return;

  float level = this->levels_[output_number];
  if (std::isnan(level)) return;
  this->lights_[output_number]->publish_feedback_level(level);
}

}  // namespace redarc_tvms_rogue
}  // namespace esphome
