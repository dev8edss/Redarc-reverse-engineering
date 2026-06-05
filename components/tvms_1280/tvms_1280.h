#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <array>
#include <vector>

namespace esphome {
namespace tvms_1280 {

class TVMS1280Component;

class TVMS1280Switch : public switch_::Switch {
 public:
  void set_parent(TVMS1280Component *parent) { this->parent_ = parent; }
  void set_output_number(uint8_t n) { this->output_number_ = n; }
  void set_channel(uint8_t ch) { this->channel_ = ch; }
  void set_is_inverter(bool v) { this->is_inverter_ = v; }
  void write_state(bool state) override;
  uint8_t output_number() const { return this->output_number_; }
  uint8_t channel() const { return this->channel_; }
  bool is_inverter() const { return this->is_inverter_; }
 protected:
  TVMS1280Component *parent_{nullptr};
  uint8_t output_number_{1};
  uint8_t channel_{0x04};
  bool is_inverter_{false};
};

class TVMS1280Component : public Component {
 public:
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_command_id(uint32_t id) { this->command_id_ = id; }
  void set_temp_tank_id(uint32_t id) { this->temp_tank_id_ = id; }
  void set_output_status_id(uint32_t id) { this->output_status_id_ = id; }
  void set_channel_status_id(uint32_t id) { this->channel_status_id_ = id; }
  void set_temp1_sensor(sensor::Sensor *s) { this->temp1_sensor_ = s; }
  void set_temp2_sensor(sensor::Sensor *s) { this->temp2_sensor_ = s; }
  void set_tank_sensor(uint8_t tank, sensor::Sensor *s) { if (tank >= 1 && tank <= 6) this->tank_sensors_[tank] = s; }
  void set_last_command_channel_sensor(sensor::Sensor *s) { this->last_command_channel_sensor_ = s; }
  void set_last_command_state_sensor(sensor::Sensor *s) { this->last_command_state_sensor_ = s; }

  void register_output_switch(TVMS1280Switch *sw);
  void register_inverter_switch(TVMS1280Switch *sw) { this->inverter_switch_ = sw; }
  void send_channel(uint8_t channel, bool state);
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  void dump_config() override;

 protected:
  void publish_output_(uint8_t output_number, bool state);
  void publish_channel_(uint8_t channel, bool state);

  canbus::Canbus *canbus_{nullptr};
  uint8_t source_address_{0x24};
  uint32_t command_id_{0x0F002420};
  uint32_t temp_tank_id_{0x1BFD0224};
  uint32_t output_status_id_{0x1BFD0024};
  uint32_t channel_status_id_{0x1BFCF024};
  std::array<TVMS1280Switch *, 11> output_switches_{};
  TVMS1280Switch *inverter_switch_{nullptr};
  sensor::Sensor *temp1_sensor_{nullptr};
  sensor::Sensor *temp2_sensor_{nullptr};
  std::array<sensor::Sensor *, 7> tank_sensors_{};
  sensor::Sensor *last_command_channel_sensor_{nullptr};
  sensor::Sensor *last_command_state_sensor_{nullptr};
};

}  // namespace tvms_1280
}  // namespace esphome
