#include "tvms_rouge.h"

namespace esphome {
namespace redarc_tvms_rouge {

static const char *const TAG = "redarc_tvms_rouge";

void TVMSRougeSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) this->parent_->send_master(state);
  this->publish_state(state);
}

light::LightTraits TVMSRougeLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void TVMSRougeLight::setup_state(light::LightState *state) {
  this->state_ = state;
}

void TVMSRougeLight::publish_feedback_level(float level_percent) {
  if (this->state_ == nullptr) return;

  if (level_percent < 0.0f) level_percent = 0.0f;
  if (level_percent > 100.0f) level_percent = 100.0f;

  const bool on = level_percent > 0.5f;
  const float brightness = on ? level_percent / 100.0f : 0.0f;

  // This is bus feedback from the RedVision/TVMS side. Update the frontend state
  // directly so we do not echo a physical-display button press back onto the CAN bus.
  this->state_->current_values.set_state(on);
  this->state_->current_values.set_brightness(brightness);
  this->state_->remote_values.set_state(on);
  this->state_->remote_values.set_brightness(brightness);
  this->state_->publish_state();
}

void TVMSRougeLight::write_state(light::LightState *state) {
  if (this->parent_ == nullptr) return;

  // ESPHome applies transitions by moving current_values toward remote_values and
  // calling write_state() for each intermediate step. Use current_values here so
  // the Rouge sees the ramp instead of jumping straight to the final target.
  const bool target_on = state->remote_values.is_on();
  const bool binary = state->current_values.is_on();
  const float brightness = state->current_values.get_brightness();

  if (!target_on) {
    if (!binary || brightness <= 0.0f) {
      this->parent_->turn_off(this->output_number_, this->channel_);
      return;
    }

    float target_percent = brightness * 100.0f;
    if (target_percent > 100.0f) target_percent = 100.0f;
    this->parent_->set_target(this->output_number_, this->channel_, target_percent);
    return;
  }

  float target_percent = brightness * 100.0f;

  // During a fade-up from OFF, ESPHome can call write_state() once at 0% before
  // the first real ramp step. Ignore that tick so we do not send an OFF command.
  if (target_percent <= 0.0f && state->remote_values.get_brightness() > 0.0f) return;

  // A plain HA turn_on can arrive as ON with brightness still at 0 because our
  // feedback publisher correctly reported the real Rouge output as OFF/0%. Do
  // not translate that into another OFF command; use current feedback if known,
  // otherwise request full ON.
  if (target_percent <= 0.0f) {
    float current = this->parent_->level(this->output_number_);
    if (!std::isnan(current) && current > this->parent_->true_off_threshold()) {
      target_percent = current;
    } else {
      target_percent = 100.0f;
    }
  }

  if (target_percent > 100.0f) target_percent = 100.0f;
  if (target_percent < this->parent_->true_off_threshold()) target_percent = this->parent_->true_off_threshold();

  this->parent_->set_target(this->output_number_, this->channel_, target_percent);
}

void TVMSRougeComponent::setup() {
  for (auto &level : this->levels_) level = NAN;
  for (auto &percent : this->last_commanded_percent_) percent = -1;
  const uint8_t sa = this->source_address_;
  const uint8_t ha = this->host_address_;
  this->output_command_id_ = 0x0F000000UL | ((uint32_t) sa << 8) | ha;
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void TVMSRougeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS Rouge SA=0x%02X HA=0x%02X", this->source_address_, this->host_address_);
  ESP_LOGCONFIG(TAG, "  Output cmd: 0x%08X  DGN: 0x1F108, 0x1FD00, 0x1FD02, 0x1FD12", this->output_command_id_);
  LOG_SENSOR("  ", "Input Voltage", this->input_voltage_sensor_);
  LOG_SENSOR("  ", "Input Current", this->input_current_sensor_);
  LOG_SWITCH("  ", "Master", this->master_switch_);
  ESP_LOGCONFIG(TAG, "  True-off threshold: %.1f%%", this->true_off_threshold_percent_);
}

void TVMSRougeComponent::register_light(TVMSRougeLight *light) {
  if (light == nullptr) return;
  uint8_t out = light->output_number();
  if (out < 1 || out > 10) return;
  this->lights_[out] = light;
}

float TVMSRougeComponent::level(uint8_t output_number) const {
  if (output_number < 1 || output_number > 10) return NAN;
  return this->levels_[output_number];
}

void TVMSRougeComponent::send_frame_(uint32_t id, const std::vector<uint8_t> &data) {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  bus->send_data(id, true, data);
}

void TVMSRougeComponent::send_on_(uint8_t channel) {
  this->send_frame_(this->output_command_id_, {0xCB, 0x00, 0xFF, channel, 0x01, 0x00, 0x00, 0x00});
}

void TVMSRougeComponent::send_off_(uint8_t channel) {
  this->send_frame_(this->output_command_id_, {0xCB, 0x00, 0xFF, channel, 0x00, 0x00, 0x00, 0x00});
}

void TVMSRougeComponent::send_level_(uint8_t channel, uint8_t percent) {
  if (percent > 100) percent = 100;
  this->send_frame_(this->output_command_id_, {0x5A, 0x01, 0xFF, channel, percent, 0x00, 0x00, 0x00});
}

void TVMSRougeComponent::send_master(bool state) {
  if (state) {
    this->send_on_(0x0B);
  } else {
    this->send_off_(0x0B);
  }
  ESP_LOGI(TAG, "Master channel 0x0B %s", state ? "ON" : "OFF");
}

void TVMSRougeComponent::turn_off(uint8_t output_number, uint8_t channel) {
  if (output_number < 1 || output_number > 10) return;
  if (this->last_commanded_percent_[output_number] == 0) return;

  this->send_off_(channel);
  this->mark_output_commanded_(output_number, 0);
  ESP_LOGI(TAG, "Output %u channel 0x%02X OFF", output_number, channel);
}

void TVMSRougeComponent::set_target(uint8_t output_number, uint8_t channel, float target_percent) {
  if (output_number < 1 || output_number > 10) return;
  if (target_percent <= this->true_off_threshold_percent_) {
    this->turn_off(output_number, channel);
    return;
  }
  if (target_percent > 100.0f) target_percent = 100.0f;

  const uint8_t percent = (uint8_t) std::round(target_percent);
  if (this->last_commanded_percent_[output_number] == percent) return;
  this->mark_output_commanded_(output_number, percent);

  if (percent >= 100) {
    this->send_on_(channel);
    ESP_LOGI(TAG, "Output %u channel 0x%02X ON", output_number, channel);
  } else {
    this->send_level_(channel, percent);
    ESP_LOGI(TAG, "Output %u channel 0x%02X level %u%%", output_number, channel, percent);
  }
}

void TVMSRougeComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (redarc_common::rvc_matches(can_id, 0x1FD02UL, this->source_address_)) {
    if (data[0] == 0x09) {
      if (now - this->last_tank_ms_ >= this->filter_interval_ms_) {
        this->last_tank_ms_ = now;
        if (this->tank1_sensor_ != nullptr) this->tank1_sensor_->publish_state((float) data[1]);
        if (this->tank2_sensor_ != nullptr) this->tank2_sensor_->publish_state((float) data[2]);
      }
    } else if (data[0] == 0x16) {
      if (now - this->last_input_current_ms_ >= this->filter_interval_ms_) {
        this->last_input_current_ms_ = now;
        if (this->input_voltage_sensor_ != nullptr) {
          const uint16_t raw_mv = (uint16_t) data[1] | ((uint16_t) data[2] << 8);
          this->input_voltage_sensor_->publish_state((float) raw_mv / 1000.0f);
        }
        if (this->input_current_sensor_ != nullptr && data[2] != 0xFF) {
          const float current = data[2] / 10.0f;
          if (current >= 0.0f && current <= 25.5f)
            this->input_current_sensor_->publish_state(current);
        }
      }
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1FD14UL, this->source_address_)) {
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
    this->handle_button_status_frame_(data);
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

void TVMSRougeComponent::publish_channel_(uint8_t channel, bool state) {
  if (channel == 0x0B) {
    if (this->master_switch_ != nullptr) this->master_switch_->publish_state(state);
    return;
  }
  if (channel < 0x0C || channel > 0x15) return;

  const uint8_t output_number = channel - 0x0B;
  if (!state) {
    this->set_feedback_level_(output_number, 0.0f);
    return;
  }

  float current = this->level(output_number);
  if (std::isnan(current) || current <= this->true_off_threshold_percent_) current = 100.0f;
  this->set_feedback_level_(output_number, current);
}

void TVMSRougeComponent::handle_button_status_frame_(const std::vector<uint8_t> &data) {
  if (data.size() < 8) return;

  const uint8_t base_input = data[0];
  for (uint8_t i = 1; i <= 7; i++) {
    const uint8_t input_number = base_input + i - 1;
    if (input_number < 1 || input_number > 8) continue;

    const uint8_t value = data[i];
    if (value != 0x00 && value != 0x01) continue;

    this->set_button_state_(input_number, value == 0x01);
  }
}

void TVMSRougeComponent::mark_output_commanded_(uint8_t output_number, int16_t percent) {
  if (output_number < 1 || output_number > 10) return;
  this->last_commanded_percent_[output_number] = percent;

  // RedVision/TVMS feedback echoes can arrive while ESPHome is still applying
  // its transition. If accepted immediately they reset current_values and make
  // the visible fade complete faster than the configured transition length.
  const uint32_t ignore_ms = this->default_transition_length_ms_ > 0 ? this->default_transition_length_ms_ + 150U : 0U;
  this->ignore_feedback_until_ms_[output_number] = millis() + ignore_ms;
}

void TVMSRougeComponent::set_button_state_(uint8_t output_number, bool active) {
  if (output_number < 1 || output_number > 8) return;
  if (this->button_state_known_[output_number] && this->button_states_[output_number] == active) return;

  this->button_state_known_[output_number] = true;
  this->button_states_[output_number] = active;
  if (this->button_sensors_[output_number] != nullptr) {
    this->button_sensors_[output_number]->publish_state(active);
  }
}

void TVMSRougeComponent::set_feedback_level_(uint8_t output_number, float level) {
  if (output_number < 1 || output_number > 10) return;
  if (this->ignore_feedback_until_ms_[output_number] != 0 &&
      (int32_t) (millis() - this->ignore_feedback_until_ms_[output_number]) < 0) {
    return;
  }
  if (level < 0.0f) level = 0.0f;
  if (level > 100.0f) level = 100.0f;
  this->last_commanded_percent_[output_number] = (int16_t) std::round(level);
  const bool changed = this->levels_[output_number] != level;
  this->levels_[output_number] = level;
  if (changed && this->level_sensors_[output_number] != nullptr)
    this->level_sensors_[output_number]->publish_state(level);
  this->publish_actual_light_level_(output_number);
}

void TVMSRougeComponent::publish_actual_light_level_(uint8_t output_number) {
  if (output_number < 1 || output_number > 10) return;
  if (this->lights_[output_number] == nullptr) return;

  float level = this->levels_[output_number];
  if (std::isnan(level)) return;
  this->lights_[output_number]->publish_feedback_level(level);
}

}  // namespace redarc_tvms_rouge
}  // namespace esphome
