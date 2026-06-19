#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <vector>
#include <cmath>

namespace esphome {
namespace redarc_battery_sensor {

class BatterySensorComponent : public Component {
 public:
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_filter_interval_ms(uint32_t ms) { this->filter_interval_ms_ = ms; }
  void set_current_sensor(sensor::Sensor *s) { this->current_sensor_ = s; }
  void set_current_raw_sensor(sensor::Sensor *s) { this->current_raw_sensor_ = s; }
  void set_voltage_sensor(sensor::Sensor *s) { this->voltage_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_soc_sensor(sensor::Sensor *s) { this->soc_sensor_ = s; }
  void set_battery_type_sensor(sensor::Sensor *s) { this->battery_type_sensor_ = s; }
  void set_configured_capacity_sensor(sensor::Sensor *s) { this->configured_capacity_sensor_ = s; }
  void set_max_charge_current_sensor(sensor::Sensor *s) { this->max_charge_current_sensor_ = s; }
  void set_low_soc_alarm_sensor(sensor::Sensor *s) { this->low_soc_alarm_sensor_ = s; }
  void set_low_voltage_alarm_sensor(sensor::Sensor *s) { this->low_voltage_alarm_sensor_ = s; }
  void set_last_soc_calibration_target_sensor(sensor::Sensor *s) { this->last_soc_calibration_target_sensor_ = s; }

  void setup() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  float current_a() const { return this->current_a_; }
  bool has_current() const { return !std::isnan(this->current_a_); }

 protected:
  uint8_t source_address_{0x08};
  uint32_t filter_interval_ms_{5000};
  uint32_t last_current_ms_{0};
  uint32_t last_soc_ms_{0};
  uint32_t last_capacity_ms_{0};
  uint32_t last_alarm_report_ms_{0};
  float current_a_{NAN};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *current_raw_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *soc_sensor_{nullptr};
  sensor::Sensor *battery_type_sensor_{nullptr};
  sensor::Sensor *configured_capacity_sensor_{nullptr};
  sensor::Sensor *max_charge_current_sensor_{nullptr};
  sensor::Sensor *low_soc_alarm_sensor_{nullptr};
  sensor::Sensor *low_voltage_alarm_sensor_{nullptr};
  sensor::Sensor *last_soc_calibration_target_sensor_{nullptr};
};

}  // namespace redarc_battery_sensor
}  // namespace esphome
