#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <vector>
#include <cmath>

namespace esphome {
namespace battery_sensor {

class BatterySensorComponent : public Component {
 public:
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_current_sensor(sensor::Sensor *s) { this->current_sensor_ = s; }
  void set_current_raw_sensor(sensor::Sensor *s) { this->current_raw_sensor_ = s; }
  void set_voltage_sensor(sensor::Sensor *s) { this->voltage_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_soc_sensor(sensor::Sensor *s) { this->soc_sensor_ = s; }

  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  float current_a() const { return this->current_a_; }
  bool has_current() const { return !std::isnan(this->current_a_); }

 protected:
  uint8_t source_address_{0x08};
  float current_a_{NAN};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *current_raw_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *soc_sensor_{nullptr};
};

}  // namespace battery_sensor
}  // namespace esphome
