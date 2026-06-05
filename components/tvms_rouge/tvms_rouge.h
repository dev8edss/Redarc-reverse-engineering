#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/sensor/sensor.h"
#include <array>
#include <cmath>
#include <vector>

namespace esphome {
namespace tvms_rouge {

class TVMSRougeComponent;

class TVMSRougeLight : public light::LightOutput {
 public:
  void set_parent(TVMSRougeComponent *parent) { this->parent_ = parent; }
  void set_output_number(uint8_t output_number) { this->output_number_ = output_number; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;
  void publish_feedback_level(float level_percent);

  uint8_t output_number() const { return this->output_number_; }
  uint8_t channel() const { return this->channel_; }

 protected:
  TVMSRougeComponent *parent_{nullptr};
  light::LightState *state_{nullptr};
  uint8_t output_number_{1};
  uint8_t channel_{0x0C};
};

class TVMSRougeComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_output_command_id(uint32_t id) { this->output_command_id_ = id; }
  void set_dim_command_id(uint32_t id) { this->dim_command_id_ = id; }
  void set_keepalive_id(uint32_t id) { this->keepalive_id_ = id; }
  void set_level_feedback_id(uint32_t id) { this->level_feedback_id_ = id; }
  void set_tank_feedback_id(uint32_t id) { this->tank_feedback_id_ = id; }
  void set_tank1_sensor(sensor::Sensor *s) { this->tank1_sensor_ = s; }
  void set_tank2_sensor(sensor::Sensor *s) { this->tank2_sensor_ = s; }
  void set_level_sensor(uint8_t output, sensor::Sensor *s) { if (output >= 1 && output <= 10) this->level_sensors_[output] = s; }

  void set_true_off_threshold(float v) { this->true_off_threshold_percent_ = v; }
  void set_target_debounce_ms(uint32_t v) { this->target_debounce_ms_ = v; }
  void set_start_deadline_ms(uint32_t v) { this->start_deadline_ms_ = v; }
  void set_deadband_percent(float v) { this->deadband_percent_ = v; }
  void set_initial_rate_ms_per_percent(float v) {
    this->initial_rate_ms_per_percent_ = v;
    if (!this->rates_initialised_) {
      this->learn_rate_up_ms_per_percent_ = v;
      this->learn_rate_down_ms_per_percent_ = v;
      this->rates_initialised_ = true;
    }
  }
  void set_learning_gain(float v) { this->learning_gain_ = v; }
  void set_approach(float v) { this->approach_ = v; }
  void set_max_pulse_ms(uint32_t v) { this->max_pulse_ms_ = v; }
  void set_settle_ms(uint32_t v) { this->settle_ms_ = v; }
  void set_max_iterations(uint8_t v) { this->max_iterations_ = v; }

  // Runtime tuning setters for optional template-number controls.
  void set_deadband(float v) { this->deadband_percent_ = v; }
  void set_initial_rate(float v) { this->initial_rate_ms_per_percent_ = v; }
  void set_learning_gain_percent(float v) { this->learning_gain_ = v / 100.0f; }
  void set_approach_percent(float v) { this->approach_ = v / 100.0f; }
  void set_max_pulse(float v) { this->max_pulse_ms_ = (uint32_t) std::round(v); }
  void set_settle_time(float v) { this->settle_ms_ = (uint32_t) std::round(v); }
  void set_max_iterations_float(float v) { this->max_iterations_ = (uint8_t) std::round(v); }

  void register_light(TVMSRougeLight *light);

  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  void set_target(uint8_t output_number, uint8_t channel, float target_percent);
  void turn_off(uint8_t output_number, uint8_t channel);
  void abort_and_release();

  float level(uint8_t output_number) const;
  float true_off_threshold() const { return this->true_off_threshold_percent_; }

 protected:
  enum State : uint8_t { IDLE = 0, WAIT_START = 1, COMPUTE = 2, HOLDING = 3, SETTLING = 4 };

  void send_frame_(uint32_t id, const std::vector<uint8_t> &data);
  void send_on_(uint8_t channel);
  void send_off_(uint8_t channel);
  void send_release_(uint8_t channel);
  void send_dim_start_(uint8_t channel, bool up);
  void send_keepalive_();

  void set_feedback_level_(uint8_t output_number, float level);
  uint8_t channel_for_output_(uint8_t output_number) const { return 0x0B + output_number; }

  canbus::Canbus *canbus_{nullptr};
  uint32_t output_command_id_{0x0F003020};
  uint32_t dim_command_id_{0x0F053020};
  uint32_t keepalive_id_{0x0FE6FF20};
  uint32_t level_feedback_id_{0x1BFD1230};
  uint32_t tank_feedback_id_{0x1BFD0230};

  std::array<float, 11> levels_{};
  std::array<TVMSRougeLight *, 11> lights_{};
  std::array<sensor::Sensor *, 11> level_sensors_{};
  sensor::Sensor *tank1_sensor_{nullptr};
  sensor::Sensor *tank2_sensor_{nullptr};

  float true_off_threshold_percent_{1.0f};
  uint32_t target_debounce_ms_{600};
  uint32_t start_deadline_ms_{2500};
  float deadband_percent_{3.0f};
  float initial_rate_ms_per_percent_{50.0f};
  float learn_rate_up_ms_per_percent_{50.0f};
  float learn_rate_down_ms_per_percent_{50.0f};
  bool rates_initialised_{false};
  float learning_gain_{0.25f};
  float approach_{0.80f};
  uint32_t max_pulse_ms_{1200};
  uint32_t settle_ms_{700};
  uint8_t max_iterations_{12};

  bool pending_active_{false};
  uint8_t pending_output_{1};
  uint8_t pending_channel_{0x0C};
  float pending_target_percent_{100.0f};
  uint32_t pending_start_ms_{0};

  bool active_{false};
  State state_{IDLE};
  uint8_t active_output_{1};
  uint8_t active_channel_{0x0C};
  float target_percent_{100.0f};
  uint8_t iteration_{0};
  uint32_t start_deadline_at_ms_{0};
  uint32_t pulse_end_ms_{0};
  uint32_t settle_end_ms_{0};
  float level_before_pulse_{NAN};
  float last_pulse_ms_{0.0f};
  bool last_direction_up_{true};
  bool learning_pending_{false};
};

}  // namespace tvms_rouge
}  // namespace esphome
