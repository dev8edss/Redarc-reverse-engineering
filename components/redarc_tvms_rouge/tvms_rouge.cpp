#include "tvms_rouge.h"

namespace esphome {
namespace redarc_tvms_rouge {

static const char *const TAG = "redarc_tvms_rouge";

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

void TVMSRougeLight::publish_target_level(float level_percent) {
  if (this->state_ == nullptr) return;

  if (level_percent < 0.0f) level_percent = 0.0f;
  if (level_percent > 100.0f) level_percent = 100.0f;

  const bool on = level_percent > 0.5f;
  const float brightness = on ? level_percent / 100.0f : 0.0f;

  // This is the HA-requested target. The Rouge now supports an absolute level
  // command, so feedback should reconcile quickly after the CAN frame is sent.
  this->state_->current_values.set_state(on);
  this->state_->current_values.set_brightness(brightness);
  this->state_->remote_values.set_state(on);
  this->state_->remote_values.set_brightness(brightness);
  this->state_->publish_state();
}

void TVMSRougeLight::write_state(light::LightState *state) {
  if (this->parent_ == nullptr) return;

  // Use the requested frontend target, not current_values. current_values may still
  // contain the previous OFF feedback when HA sends a plain turn_on command.
  bool binary = state->remote_values.is_on();
  float brightness = state->remote_values.get_brightness();

  if (!binary) {
    this->publish_target_level(0.0f);
    this->parent_->turn_off(this->output_number_, this->channel_);
    return;
  }

  float target_percent = brightness * 100.0f;

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

  this->publish_target_level(target_percent);
  this->parent_->set_target(this->output_number_, this->channel_, target_percent);
}

void TVMSRougeComponent::setup() {
  for (auto &level : this->levels_) level = NAN;
  const uint8_t sa = this->source_address_;
  const uint8_t ha = this->host_address_;
  this->output_command_id_ = 0x0F000000UL | ((uint32_t) sa << 8) | ha;
  this->level_feedback_id_ = 0x1BFD1200UL | sa;
  this->tank_feedback_id_  = 0x1BFD0200UL | sa;
  this->button_status_id_  = 0x1BFD1400UL | sa;
  this->input_status_id_   = 0x13F10800UL | sa;
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void TVMSRougeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS Rouge SA=0x%02X HA=0x%02X", this->source_address_, this->host_address_);
  ESP_LOGCONFIG(TAG, "  Output cmd: 0x%08X  Level: 0x%08X", this->output_command_id_, this->level_feedback_id_);
  LOG_SENSOR("  ", "Input Voltage", this->input_voltage_sensor_);
  LOG_SENSOR("  ", "Input Current", this->input_current_sensor_);
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

void TVMSRougeComponent::turn_off(uint8_t output_number, uint8_t channel) {
  if (output_number < 1 || output_number > 10) return;

  this->send_off_(channel);
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
  if (percent >= 100) {
    this->send_on_(channel);
    ESP_LOGI(TAG, "Output %u channel 0x%02X ON", output_number, channel);
  } else {
    this->send_level_(channel, percent);
    ESP_LOGI(TAG, "Output %u channel 0x%02X level %u%%", output_number, channel, percent);
  }
}

void TVMSRougeComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id &= 0x1FFFFFFFUL;
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (can_id == this->tank_feedback_id_) {
    if (data[0] == 0x09) {
      if (now - this->last_tank_ms_ >= this->filter_interval_ms_) {
        this->last_tank_ms_ = now;
        if (this->tank1_sensor_ != nullptr) this->tank1_sensor_->publish_state((float) data[1]);
        if (this->tank2_sensor_ != nullptr) this->tank2_sensor_->publish_state((float) data[2]);
      }
    } else if (data[0] == 0x16) {
      // Label pages identify item 0x16 as Input Voltage and item 0x17 as
      // Input Current. In the grouped 0x1BFD0230 status frame, D1 is the base
      // item, so D3 carries item 0x17. Observed D3=0x2F => 4.7 A.
      if (now - this->last_input_current_ms_ >= this->filter_interval_ms_) {
        this->last_input_current_ms_ = now;
        if (this->input_current_sensor_ != nullptr && data[2] != 0xFF) {
          const float current = data[2] / 10.0f;
          if (current >= 0.0f && current <= 25.5f)
            this->input_current_sensor_->publish_state(current);
        }
      }
    }
    return;
  }

  if (can_id == this->button_status_id_) {
    // Button state changes are not throttled; they are edge events.
    this->handle_button_status_frame_(data);
    return;
  }

  if (can_id == this->input_status_id_) {
    if (now - this->last_input_status_ms_ >= this->filter_interval_ms_) {
      this->last_input_status_ms_ = now;
      this->handle_input_status_frame_(data);
    }
    return;
  }

  if (can_id == this->output_command_id_) {
    // Output command events are not throttled; they reflect requested state changes.
    if (data[0] == 0xCB && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      if (channel >= 0x0C && channel <= 0x15) {
        const uint8_t output_number = channel - 0x0B;
        const bool on = data[4] != 0;
        if (!on) {
          this->set_feedback_level_(output_number, 0.0f);
        } else {
          float current = this->level(output_number);
          if (std::isnan(current) || current <= this->true_off_threshold_percent_) current = 100.0f;
          this->set_feedback_level_(output_number, current);
        }
      }
    } else if (data[0] == 0x5A && data[1] == 0x01 && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      if (channel >= 0x0C && channel <= 0x15 && data[4] <= 100) {
        const uint8_t output_number = channel - 0x0B;
        this->set_feedback_level_(output_number, (float) data[4]);
      }
    }
    return;
  }

  if (can_id != this->level_feedback_id_) return;

  if (data[0] == 0x0C) {
    for (uint8_t i = 1; i <= 7; i++) this->set_feedback_level_(i, (float) data[i]);
  } else if (data[0] == 0x13) {
    this->set_feedback_level_(8, (float) data[1]);
    this->set_feedback_level_(9, (float) data[2]);
    this->set_feedback_level_(10, (float) data[3]);
  }
}

void TVMSRougeComponent::handle_button_status_frame_(const std::vector<uint8_t> &data) {
  if (data.size() < 8) return;

  const uint8_t base_channel = data[0];
  for (uint8_t i = 1; i <= 7; i++) {
    const uint8_t channel = base_channel + i - 1;
    if (channel < 0x0C || channel > 0x15) continue;

    const uint8_t value = data[i];
    if (value != 0x00 && value != 0x02) continue;

    const uint8_t output_number = channel - 0x0B;
    this->set_button_state_(output_number, value == 0x02);
  }
}

void TVMSRougeComponent::handle_input_status_frame_(const std::vector<uint8_t> &data) {
  if (data.size() < 2) return;
  if (this->input_voltage_sensor_ == nullptr) return;

  const uint16_t raw = (uint16_t) data[0] | ((uint16_t) data[1] << 8);
  if (raw == 0x0000 || raw == 0xFFFF) return;

  const float voltage = raw / 100.0f;
  if (voltage < 0.0f || voltage > 60.0f) return;

  this->input_voltage_sensor_->publish_state(voltage);
}

void TVMSRougeComponent::set_button_state_(uint8_t output_number, bool active) {
  if (output_number < 1 || output_number > 10) return;
  if (this->button_state_known_[output_number] && this->button_states_[output_number] == active) return;

  this->button_state_known_[output_number] = true;
  this->button_states_[output_number] = active;
  if (this->button_sensors_[output_number] != nullptr) {
    this->button_sensors_[output_number]->publish_state(active);
  }
}

void TVMSRougeComponent::set_feedback_level_(uint8_t output_number, float level) {
  if (output_number < 1 || output_number > 10) return;
  if (level < 0.0f) level = 0.0f;
  if (level > 100.0f) level = 100.0f;
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
