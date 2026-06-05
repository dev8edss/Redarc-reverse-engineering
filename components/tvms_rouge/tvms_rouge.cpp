#include "tvms_rouge.h"

namespace esphome {
namespace tvms_rouge {

static const char *const TAG = "tvms_rouge";

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

  // Use the requested frontend target, not current_values. current_values may still
  // contain the previous OFF feedback when HA sends a plain turn_on command.
  bool binary = state->remote_values.is_on();
  float brightness = state->remote_values.get_brightness();

  if (!binary) {
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
  this->parent_->set_target(this->output_number_, this->channel_, target_percent);
}

void TVMSRougeComponent::setup() {
  for (auto &level : this->levels_) level = NAN;
}

void TVMSRougeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS Rouge external component:");
  ESP_LOGCONFIG(TAG, "  Output command: 0x%08X", this->output_command_id_);
  ESP_LOGCONFIG(TAG, "  Dim command: 0x%08X", this->dim_command_id_);
  ESP_LOGCONFIG(TAG, "  Keepalive: 0x%08X", this->keepalive_id_);
  ESP_LOGCONFIG(TAG, "  Level feedback: 0x%08X", this->level_feedback_id_);
  ESP_LOGCONFIG(TAG, "  Tank feedback: 0x%08X", this->tank_feedback_id_);
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
  if (this->canbus_ == nullptr) return;
  this->canbus_->send_data(id, true, data);
}

void TVMSRougeComponent::send_on_(uint8_t channel) {
  this->send_frame_(this->output_command_id_, {0xCB, 0x00, 0xFF, channel, 0x01, 0x00, 0x00, 0x00});
}

void TVMSRougeComponent::send_off_(uint8_t channel) {
  this->send_frame_(this->output_command_id_, {0xCB, 0x00, 0xFF, channel, 0x00, 0x00, 0x00, 0x00});
}

void TVMSRougeComponent::send_release_(uint8_t channel) {
  this->send_frame_(this->dim_command_id_, {channel, 0x01, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF});
}

void TVMSRougeComponent::send_dim_start_(uint8_t channel, bool up) {
  this->send_frame_(this->dim_command_id_, {channel, 0x01, (uint8_t) (up ? 0x64 : 0x01), 0x05, 0x00, 0xFF, 0xFF, 0xFF});
}

void TVMSRougeComponent::send_keepalive_() {
  this->send_frame_(this->keepalive_id_, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

void TVMSRougeComponent::abort_and_release() {
  if (this->active_) this->send_release_(this->active_channel_);
  for (uint8_t output = 1; output <= 10; output++) this->send_release_(this->channel_for_output_(output));
  this->active_ = false;
  this->pending_active_ = false;
  this->state_ = IDLE;
  this->learning_pending_ = false;
}

void TVMSRougeComponent::turn_off(uint8_t output_number, uint8_t channel) {
  if (output_number < 1 || output_number > 10) return;

  if (this->active_ && this->active_output_ == output_number) this->send_release_(this->active_channel_);
  this->active_ = false;
  this->pending_active_ = false;
  this->state_ = IDLE;
  this->learning_pending_ = false;

  this->send_release_(channel);
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

  uint32_t now = millis();

  // If output is off/no feedback, turn it on first. The PID will then ramp from whatever
  // stored/default level the Rouge reports. This only happens from OFF, not before every target.
  float current = this->level(output_number);
  if (std::isnan(current) || current <= this->true_off_threshold_percent_) {
    this->send_on_(channel);
    this->start_deadline_at_ms_ = now + this->start_deadline_ms_;
  } else {
    this->start_deadline_at_ms_ = now + 300;
  }

  // Coalesce HA brightness writes. This prevents gamma/transition intermediate targets
  // from starting a separate ramp.
  this->pending_output_ = output_number;
  this->pending_channel_ = channel;
  this->pending_target_percent_ = target_percent;
  this->pending_start_ms_ = now + this->target_debounce_ms_;
  this->pending_active_ = true;

  ESP_LOGI(TAG, "Queued target output %u channel 0x%02X %.1f%%", output_number, channel, target_percent);
}

void TVMSRougeComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id &= 0x1FFFFFFFUL;
  if (data.size() < 8) return;

  if (can_id == this->tank_feedback_id_) {
    if (data[0] == 0x09) {
      if (this->tank1_sensor_ != nullptr) this->tank1_sensor_->publish_state((float) data[1]);
      if (this->tank2_sensor_ != nullptr) this->tank2_sensor_->publish_state((float) data[2]);
    }
    return;
  }

  if (can_id == this->output_command_id_) {
    if (data[0] == 0xCB && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      if (channel >= 0x0C && channel <= 0x15) {
        const uint8_t output_number = channel - 0x0B;

        // Do not let an echoed copy of our own HA-originated command shortcut the
        // dimming state machine; real Rouge level feedback remains authoritative.
        const bool local_command_in_progress =
            (this->active_ && output_number == this->active_output_) ||
            (this->pending_active_ && output_number == this->pending_output_);
        if (local_command_in_progress) return;

        const bool on = data[4] != 0;
        if (!on) {
          this->set_feedback_level_(output_number, 0.0f);
        } else {
          float current = this->level(output_number);
          if (std::isnan(current) || current <= this->true_off_threshold_percent_) current = 100.0f;
          this->set_feedback_level_(output_number, current);
        }
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

void TVMSRougeComponent::set_feedback_level_(uint8_t output_number, float level) {
  if (output_number < 1 || output_number > 10) return;
  if (level < 0.0f) level = 0.0f;
  if (level > 100.0f) level = 100.0f;
  this->levels_[output_number] = level;
  if (this->level_sensors_[output_number] != nullptr) this->level_sensors_[output_number]->publish_state(level);
  if (this->lights_[output_number] != nullptr) this->lights_[output_number]->publish_feedback_level(level);
}

void TVMSRougeComponent::loop() {
  const uint32_t now = millis();

  if (!this->active_) {
    if (!this->pending_active_) return;
    if ((int32_t) (now - this->pending_start_ms_) < 0) return;

    this->active_output_ = this->pending_output_;
    this->active_channel_ = this->pending_channel_;
    this->target_percent_ = this->pending_target_percent_;
    this->iteration_ = 0;
    this->learning_pending_ = false;
    this->pending_active_ = false;
    this->active_ = true;
    this->state_ = WAIT_START;
    ESP_LOGI(TAG, "Starting target output %u channel 0x%02X %.1f%%", this->active_output_, this->active_channel_, this->target_percent_);
  }

  const uint8_t out = this->active_output_;
  const uint8_t ch = this->active_channel_;

  if (this->state_ == WAIT_START) {
    this->send_release_(ch);
    float current = this->level(out);
    if (std::isnan(current) || current <= this->true_off_threshold_percent_) {
      if ((int32_t) (now - this->start_deadline_at_ms_) < 0) return;
      ESP_LOGW(TAG, "Output %u still off/no feedback %.1f; target aborted", out, current);
      this->active_ = false;
      this->state_ = IDLE;
      this->learning_pending_ = false;
      return;
    }
    this->settle_end_ms_ = now + 250;
    this->state_ = SETTLING;
    return;
  }

  if (this->state_ == HOLDING) {
    this->send_keepalive_();
    if ((int32_t) (now - this->pulse_end_ms_) >= 0) {
      this->send_release_(ch);
      this->settle_end_ms_ = now + this->settle_ms_;
      this->state_ = SETTLING;
    }
    return;
  }

  if (this->state_ == SETTLING) {
    if ((int32_t) (now - this->settle_end_ms_) < 0) return;

    if (this->learning_pending_) {
      float after = this->level(out);
      float before = this->level_before_pulse_;
      if (!std::isnan(after) && !std::isnan(before)) {
        float delta = after - before;
        bool expected = (this->last_direction_up_ && delta > 0.5f) || (!this->last_direction_up_ && delta < -0.5f);
        if (expected && std::fabs(delta) >= 1.0f && this->last_pulse_ms_ > 0.0f) {
          float observed_rate = this->last_pulse_ms_ / std::fabs(delta);
          if (observed_rate < 20.0f) observed_rate = 20.0f;
          if (observed_rate > 160.0f) observed_rate = 160.0f;
          float alpha = this->learning_gain_;
          if (alpha < 0.01f) alpha = 0.01f;
          if (alpha > 1.0f) alpha = 1.0f;
          if (this->last_direction_up_) {
            this->learn_rate_up_ms_per_percent_ += alpha * (observed_rate - this->learn_rate_up_ms_per_percent_);
          } else {
            this->learn_rate_down_ms_per_percent_ += alpha * (observed_rate - this->learn_rate_down_ms_per_percent_);
          }
          ESP_LOGI(TAG, "learn output %u before %.1f after %.1f delta %.1f pulse %.0f ms observed %.1f ms/%% up %.1f down %.1f", out, before, after, delta, this->last_pulse_ms_, observed_rate, this->learn_rate_up_ms_per_percent_, this->learn_rate_down_ms_per_percent_);
        }
      }
      this->learning_pending_ = false;
    }
    this->state_ = COMPUTE;
  }

  if (this->state_ == COMPUTE) {
    if (this->iteration_ >= this->max_iterations_) {
      this->send_release_(ch);
      ESP_LOGW(TAG, "Max iterations reached; release sent for output %u", out);
      this->active_ = false;
      this->state_ = IDLE;
      this->learning_pending_ = false;
      return;
    }

    float current = this->level(out);
    if (std::isnan(current)) {
      this->settle_end_ms_ = now + 500;
      this->state_ = SETTLING;
      ESP_LOGW(TAG, "No Rouge level feedback yet for output %u", out);
      return;
    }

    float target = this->target_percent_;
    if (target < this->true_off_threshold_percent_) target = this->true_off_threshold_percent_;
    if (target > 100.0f) target = 100.0f;

    float error = target - current;
    if (std::fabs(error) <= this->deadband_percent_) {
      this->send_release_(ch);
      ESP_LOGI(TAG, "Reached target output %u current %.1f target %.1f", out, current, target);
      this->active_ = false;
      this->state_ = IDLE;
      this->learning_pending_ = false;
      return;
    }

    bool up = error > 0.0f;
    float rate = up ? this->learn_rate_up_ms_per_percent_ : this->learn_rate_down_ms_per_percent_;
    if (std::isnan(rate) || rate < 20.0f || rate > 160.0f) rate = this->initial_rate_ms_per_percent_;
    if (rate < 20.0f) rate = 20.0f;
    if (rate > 160.0f) rate = 160.0f;

    float approach = this->approach_;
    if (approach < 0.25f) approach = 0.25f;
    if (approach > 1.0f) approach = 1.0f;

    float pulse_f = std::fabs(error) * rate * approach;
    if (pulse_f < 180.0f) pulse_f = 180.0f;
    if (pulse_f > (float) this->max_pulse_ms_) pulse_f = (float) this->max_pulse_ms_;
    uint32_t pulse_ms = (uint32_t) std::round(pulse_f);

    this->level_before_pulse_ = current;
    this->last_pulse_ms_ = (float) pulse_ms;
    this->last_direction_up_ = up;
    this->learning_pending_ = true;

    ESP_LOGI(TAG, "iter %u output %u current %.1f target %.1f error %.1f rate %.1f pulse %u ms %s", this->iteration_ + 1, out, current, target, error, rate, pulse_ms, up ? "UP" : "DOWN");
    this->send_dim_start_(ch, up);
    this->send_keepalive_();
    this->pulse_end_ms_ = now + pulse_ms;
    this->iteration_++;
    this->state_ = HOLDING;
    return;
  }

  this->send_release_(ch);
  this->active_ = false;
  this->learning_pending_ = false;
  this->state_ = IDLE;
}

}  // namespace tvms_rouge
}  // namespace esphome
