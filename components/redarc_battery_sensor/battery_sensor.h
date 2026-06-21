#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/button/button.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <array>
#include <vector>
#include <cmath>

namespace esphome {
namespace redarc_battery_sensor {

class BatterySensorComponent;

class BatterySOCCalibrateButton : public button::Button {
 public:
  void set_parent(BatterySensorComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  BatterySensorComponent *parent_{nullptr};
};

class BatteryConfigNumber : public number::Number {
 public:
  void set_parent(BatterySensorComponent *parent) { this->parent_ = parent; }
  void set_command(uint8_t command) { this->command_ = command; }
  void set_raw_multiplier(float multiplier) { this->raw_multiplier_ = multiplier; }

 protected:
  void control(float value) override;
  BatterySensorComponent *parent_{nullptr};
  uint8_t command_{0};
  float raw_multiplier_{1.0f};
};

class BatteryTypeSelect : public select::Select {
 public:
  void set_parent(BatterySensorComponent *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;
  BatterySensorComponent *parent_{nullptr};
};

class BatterySensorComponent : public Component {
 public:
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_filter_interval_ms(uint32_t ms) { this->filter_interval_ms_ = ms; }
  void set_soc_history_poll_interval_ms(uint32_t ms) { this->soc_history_poll_interval_ms_ = ms; }
  void set_current_sensor(sensor::Sensor *s) { this->current_sensor_ = s; }
  void set_voltage_sensor(sensor::Sensor *s) { this->voltage_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_soc_sensor(sensor::Sensor *s) { this->soc_sensor_ = s; }
  void set_battery_type_sensor(sensor::Sensor *s) { this->battery_type_sensor_ = s; }
  void set_configured_capacity_sensor(sensor::Sensor *s) { this->configured_capacity_sensor_ = s; }
  void set_max_charge_current_sensor(sensor::Sensor *s) { this->max_charge_current_sensor_ = s; }
  void set_low_soc_alarm_sensor(sensor::Sensor *s) { this->low_soc_alarm_sensor_ = s; }
  void set_low_voltage_alarm_sensor(sensor::Sensor *s) { this->low_voltage_alarm_sensor_ = s; }
  void set_last_soc_calibration_target_sensor(sensor::Sensor *s) { this->last_soc_calibration_target_sensor_ = s; }
  void set_battery_type_select(BatteryTypeSelect *s) { this->battery_type_select_ = s; }
  void set_configured_capacity_number(number::Number *n) { this->configured_capacity_number_ = n; }
  void set_max_charge_current_number(number::Number *n) { this->max_charge_current_number_ = n; }
  void set_low_soc_alarm_number(number::Number *n) { this->low_soc_alarm_number_ = n; }
  void set_low_voltage_alarm_number(number::Number *n) { this->low_voltage_alarm_number_ = n; }
  void set_soc_calibration_button(BatterySOCCalibrateButton *b) { this->soc_calibration_button_ = b; }
  void set_soc_hourly_history_text_sensor(text_sensor::TextSensor *s) { this->soc_hourly_history_text_sensor_ = s; }
  void set_soc_daily_low_history_text_sensor(text_sensor::TextSensor *s) { this->soc_daily_low_history_text_sensor_ = s; }
  void set_soc_daily_high_history_text_sensor(text_sensor::TextSensor *s) { this->soc_daily_high_history_text_sensor_ = s; }
  void set_soc_daily_range_history_text_sensor(text_sensor::TextSensor *s) { this->soc_daily_range_history_text_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  void send_config_setting(uint8_t command, uint16_t raw_value);
  void send_soc_calibration_command(uint8_t target_percent);
  float current_a() const { return this->current_a_; }
  bool has_current() const { return !std::isnan(this->current_a_); }

 protected:
  void request_soc_history_();
  bool send_pending_soc_history_request_(uint32_t now);
  void send_history_preamble_();
  void handle_soc_history_page_(uint32_t dgn, const std::vector<uint8_t> &data);
  void publish_soc_hourly_history_();
  void publish_soc_daily_history_();
  void append_csv_value_(char *buffer, size_t buffer_size, size_t &used, uint8_t value, bool &first);
  void append_csv_range_(char *buffer, size_t buffer_size, size_t &used, uint8_t low, uint8_t high, bool &first);

  uint8_t source_address_{0x08};
  uint32_t filter_interval_ms_{5000};
  uint32_t soc_history_poll_interval_ms_{60000};
  uint32_t last_soc_history_poll_ms_{0};
  uint32_t last_soc_history_request_ms_{0};
  uint32_t pending_soc_history_preamble_ms_{0};
  bool pending_soc_history_poll_{false};
  uint8_t pending_soc_history_request_index_{0xFF};
  uint32_t last_current_ms_{0};
  uint32_t last_soc_ms_{0};
  uint32_t last_capacity_ms_{0};
  uint32_t last_alarm_report_ms_{0};
  float current_a_{NAN};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *soc_sensor_{nullptr};
  sensor::Sensor *battery_type_sensor_{nullptr};
  sensor::Sensor *configured_capacity_sensor_{nullptr};
  sensor::Sensor *max_charge_current_sensor_{nullptr};
  sensor::Sensor *low_soc_alarm_sensor_{nullptr};
  sensor::Sensor *low_voltage_alarm_sensor_{nullptr};
  sensor::Sensor *last_soc_calibration_target_sensor_{nullptr};
  BatteryTypeSelect *battery_type_select_{nullptr};
  number::Number *configured_capacity_number_{nullptr};
  number::Number *max_charge_current_number_{nullptr};
  number::Number *low_soc_alarm_number_{nullptr};
  number::Number *low_voltage_alarm_number_{nullptr};
  BatterySOCCalibrateButton *soc_calibration_button_{nullptr};
  text_sensor::TextSensor *soc_hourly_history_text_sensor_{nullptr};
  text_sensor::TextSensor *soc_daily_low_history_text_sensor_{nullptr};
  text_sensor::TextSensor *soc_daily_high_history_text_sensor_{nullptr};
  text_sensor::TextSensor *soc_daily_range_history_text_sensor_{nullptr};
  std::array<uint8_t, 28> soc_hourly_history_{};
  std::array<uint8_t, 35> soc_daily_low_history_{};
  std::array<uint8_t, 35> soc_daily_high_history_{};
};

}  // namespace redarc_battery_sensor
}  // namespace esphome
