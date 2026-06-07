#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/redarc_battery_sensor/battery_sensor.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <vector>

namespace esphome {
namespace redarc_manager {

class Manager30Component : public Component {
 public:
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_battery_sensor(redarc_battery_sensor::BatterySensorComponent *b) { this->battery_sensor_ = b; }
  void set_output_current_sensor(sensor::Sensor *s) { this->output_current_sensor_ = s; }
  void set_output_current_raw_sensor(sensor::Sensor *s) { this->output_current_raw_sensor_ = s; }
  void set_battery_voltage_sensor(sensor::Sensor *s) { this->battery_voltage_sensor_ = s; }
  void set_source_device_current_sensor(sensor::Sensor *s) { this->source_device_current_sensor_ = s; }
  void set_solar_current_sensor(sensor::Sensor *s) { this->solar_current_sensor_ = s; }
  void set_solar_voltage_sensor(sensor::Sensor *s) { this->solar_voltage_sensor_ = s; }
  void set_solar_power_sensor(sensor::Sensor *s) { this->solar_power_sensor_ = s; }
  void set_solar_energy_sensor(sensor::Sensor *s) { this->solar_energy_sensor_ = s; }
  void set_ac_input_voltage_sensor(sensor::Sensor *s) { this->ac_input_voltage_sensor_ = s; }

  void setup() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);

 protected:
  uint8_t source_address_{0x01};
  redarc_battery_sensor::BatterySensorComponent *battery_sensor_{nullptr};
  sensor::Sensor *output_current_sensor_{nullptr};
  sensor::Sensor *output_current_raw_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *source_device_current_sensor_{nullptr};
  sensor::Sensor *solar_current_sensor_{nullptr};
  sensor::Sensor *solar_voltage_sensor_{nullptr};
  sensor::Sensor *solar_power_sensor_{nullptr};
  sensor::Sensor *solar_energy_sensor_{nullptr};
  sensor::Sensor *ac_input_voltage_sensor_{nullptr};
};

}  // namespace redarc_manager
}  // namespace esphome
