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
  if (this->dimmable_) {
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  } else {
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  }
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
  this->last_sent_percent_ = on ? (this->dimmable_ ? (uint8_t) std::round(level_percent) : 100U) : 0U;

  if (this->dimmable_ && this->state_->is_transformer_active()) return;

  this->state_->current_values.set_state(on);
  this->state_->remote_values.set_state(on);

  if (this->dimmable_) {
    if (on) this->last_nonzero_level_percent_ = level_percent;
    const float retained_brightness = this->last_nonzero_level_percent_ / 100.0f;
    this->state_->current_values.set_brightness(on ? level_percent / 100.0f : retained_brightness);
    this->state_->remote_values.set_brightness(on ? level_percent / 100.0f : retained_brightness);
  }

  this->state_->publish_state();
}

void TVMSRogueLight::write_state(light::LightState *state) {
  if (this->parent_ == nullptr) return;

  const bool target_on = state->remote_values.is_on();

  // Non-dimmable Rogue channels remain light entities, but expose only ON/OFF
  // and never send absolute-level commands or transition steps.
  if (!this->dimmable_) {
    const uint8_t requested = target_on ? 100U : 0U;
    if (requested == this->last_sent_percent_) return;
    this->last_sent_percent_ = requested;
    if (target_on) {
      this->parent_->set_target(this->output_number_, this->channel_, 100.0f);
    } else {
      this->parent_->turn_off(this->output_number_, this->channel_);
    }
    return;
  }

  // ESPHome updates current_values on every transition step. remote_values is the
  // final requested target and using it for brightness would make transitions instant.
  bool current_on = false;
  float current_brightness = 0.0f;
  state->current_values_as_binary(&current_on);
  state->current_values_as_brightness(&current_brightness);

  const float target_brightness = state->remote_values.get_brightness();

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

  if (!target_on && (!current_on || current_percent <= this->parent_->true_off_threshold())) {
    if (this->last_sent_percent_ == 0) return;
    this->last_sent_percent_ = 0;
    this->parent_->turn_off(this->output_number_, this->channel_);
    return;
  }

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
  redarc_common::RedarcCanDispatcher::instance().add_logger_connected_listener(
      [this]() { this->on_logger_connected_(); });
}

void TVMSRogueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS Rogue SA=0x%02X HA=0x%02X", this->source_address_, this->host_address_);
  ESP_LOGCONFIG(TAG, "  Output cmd: 0x%08X  DGN: 0x1FD00, 0x1FD02, 0x1FD0E, 0x1FD12", this->output_command_id_);
  LOG_SENSOR("  ", "Input Voltage", this->input_voltage_sensor_);
  LOG_SENSOR("  ", "Input Current", this->input_current_sensor_);
  LOG_TEXT_SENSOR("  ", "Output Status", this->output_status_text_sensor_);
  LOG_SWITCH("  ", "Master", this->master_switch_);
  ESP_LOGCONFIG(TAG, "  True-off threshold: %.1f%%", this->true_off_threshold_percent_);

  std::string configured;
  for (uint8_t output = 1; output <= 10; output++) {
    if (!this->configured_dimmable_[output]) continue;
    if (!configured.empty()) configured += ",";
    configured += std::to_string(output);
  }
  if (configured.empty()) configured = "none";
  ESP_LOGCONFIG(TAG, "  Dimmable outputs: %s", configured.c_str());
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

void TVMSRogueComponent::on_logger_connected_() {
  this->set_timeout("rogue_output_config_request", 300, [this]() { this->request_output_configuration_(); });
}

void TVMSRogueComponent::request_output_configuration_() {
  this->reported_dimmable_known_.fill(false);
  this->reported_dimmable_.fill(false);
  this->reported_config_raw_.fill(0xFF);
  this->output_config_request_pending_ = true;

  const uint32_t request_id =
      0x0F030000UL | ((uint32_t) this->source_address_ << 8) | this->host_address_;
  this->send_frame_(request_id, {0x0E, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  ESP_LOGI(TAG, "Requested Rogue output configuration (DGN 0x1FD0E)");

  this->set_timeout("rogue_output_config_validate", 2500,
                    [this]() { this->validate_output_configuration_(); });
}

void TVMSRogueComponent::handle_output_configuration_frame_(const std::vector<uint8_t> &data) {
  if (!this->output_config_request_pending_ || data.size() < 2) return;
  const uint8_t channel = data[0];
  if (channel < ROGUE_ITEM_OUTPUT_1 || channel > ROGUE_ITEM_OUTPUT_10) return;

  const uint8_t output = channel - ROGUE_ITEM_MASTER;
  this->reported_config_raw_[output] = data[1];
  this->reported_dimmable_[output] = (data[1] & 0x80U) != 0;
  this->reported_dimmable_known_[output] = true;
}

void TVMSRogueComponent::validate_output_configuration_() {
  this->output_config_request_pending_ = false;
  uint8_t received = 0;
  uint8_t mismatches = 0;

  for (uint8_t output = 1; output <= 10; output++) {
    if (!this->reported_dimmable_known_[output]) continue;
    received++;
    if (this->reported_dimmable_[output] == this->configured_dimmable_[output]) continue;
    mismatches++;
    ESP_LOGW(TAG,
             "Output %u dimming mismatch: YAML=%s Rogue=%s (0x1FD0E D2=0x%02X)",
             (unsigned) output,
             this->configured_dimmable_[output] ? "dimmable" : "on/off",
             this->reported_dimmable_[output] ? "dimmable" : "on/off",
             (unsigned) this->reported_config_raw_[output]);
  }

  if (received == 0) {
    ESP_LOGW(TAG, "No DGN 0x1FD0E response; Rogue dimming configuration could not be validated");
    return;
  }
  if (received < 10) {
    ESP_LOGW(TAG, "Received Rogue configuration for %u/10 outputs; validation is incomplete",
             (unsigned) received);
  }
  if (mismatches == 0) {
    ESP_LOGI(TAG, "Rogue dimming configuration matches YAML for %u reported outputs",
             (unsigned) received);
  }
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

  if (redarc_common::rvc_matches(can_id, 0x1FD0EUL, this->source_address_)) {
    this->handle_output_configuration_frame_(data);
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FD02UL, this->source_address_)) {
    if (data[0] == ROGUE_ITEM_TANK_1) {
      if (now - this->last_tank_ms_ >= this->filter_interval_ms_) {
        this->last_tank_ms_ = now;
        if (this->tank1_sensor_ != nullptr) this->tank1_sensor_->publish_state((float) data[1]);
        if (this->tank2_sensor_ != nullptr) this->tank2_sensor_->publish_state((float) data[2]);
      }
    } else if (data[0] == ROGUE_ITEM_INPUT_VOLTAGE) {
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
    if (st == nullptr) continue;
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
