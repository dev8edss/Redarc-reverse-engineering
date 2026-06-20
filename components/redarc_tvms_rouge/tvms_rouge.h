#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <array>
#include <cmath>
#include <vector>

namespace esphome {
namespace redarc_tvms_rouge {

class TVMSRougeComponent;

class TVMSRougeSwitch : public switch_::Switch {
 public:
  void set_parent(TVMSRougeComponent *parent) { this->parent_ = parent; }
  void write_state(bool state) override;

 protected:
  TVMSRougeComponent *parent_{nullptr};
};

class TVMSRougeLight : public light::LightOutput {
 public:
  void set_parent(TVMSRougeComponent *parent) { this->parent_ = parent; }
  void set_output_number(uint8_t output_number) { this->output_number_ = output_number; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;
  void publish_feedback_level(float level_percent);
  void publish_target_level(float level_percent);

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
  void dump_config() override;

  void set_source_address(uint8_t sa) { this->source_address_ = sa; }
  void set_host_address(uint8_t ha) { this->host_address_ = ha; }
  void set_filter_interval_ms(uint32_t ms) { this->filter_interval_ms_ = ms; }
  void set_tank1_sensor(sensor::Sensor *s) { this->tank1_sensor_ = s; }
  void set_tank2_sensor(sensor::Sensor *s) { this->tank2_sensor_ = s; }
  void set_input_voltage_sensor(sensor::Sensor *s) { this->input_voltage_sensor_ = s; }
  void set_input_current_sensor(sensor::Sensor *s) { this->input_current_sensor_ = s; }
  void set_level_sensor(uint8_t output, sensor::Sensor *s) { if (output >= 1 && output <= 10) this->level_sensors_[output] = s; }
  void set_button_sensor(uint8_t output, binary_sensor::BinarySensor *s) { if (output >= 1 && output <= 10) this->button_sensors_[output] = s; }

  void set_true_off_threshold(float v) { this->true_off_threshold_percent_ = v; }

  void register_light(TVMSRougeLight *light);
  void register_master_switch(TVMSRougeSwitch *sw) { this->master_switch_ = sw; }

  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  void set_target(uint8_t output_number, uint8_t channel, float target_percent);
  void turn_off(uint8_t output_number, uint8_t channel);
  void send_master(bool state);

  float level(uint8_t output_number) const;
  float true_off_threshold() const { return this->true_off_threshold_percent_; }

 protected:
  void send_frame_(uint32_t id, const std::vector<uint8_t> &data);
  void send_on_(uint8_t channel);
  void send_off_(uint8_t channel);
  void send_level_(uint8_t channel, uint8_t percent);

  void publish_channel_(uint8_t channel, bool state);
  void set_feedback_level_(uint8_t output_number, float level);
  void publish_actual_light_level_(uint8_t output_number);
  void set_button_state_(uint8_t output_number, bool active);
  void handle_button_status_frame_(const std::vector<uint8_t> &data);

  uint8_t source_address_{0x30};
  uint8_t host_address_{0x20};
  uint32_t filter_interval_ms_{5000};
  uint32_t last_tank_ms_{0};
  uint32_t last_input_current_ms_{0};
  uint32_t output_command_id_{0};

  std::array<float, 11> levels_{};
  std::array<TVMSRougeLight *, 11> lights_{};
  TVMSRougeSwitch *master_switch_{nullptr};
  std::array<sensor::Sensor *, 11> level_sensors_{};
  std::array<binary_sensor::BinarySensor *, 11> button_sensors_{};
  std::array<bool, 11> button_states_{};
  std::array<bool, 11> button_state_known_{};
  sensor::Sensor *tank1_sensor_{nullptr};
  sensor::Sensor *tank2_sensor_{nullptr};
  sensor::Sensor *input_voltage_sensor_{nullptr};
  sensor::Sensor *input_current_sensor_{nullptr};

  float true_off_threshold_percent_{1.5f};
};

}  // namespace redarc_tvms_rouge
}  // namespace esphome
