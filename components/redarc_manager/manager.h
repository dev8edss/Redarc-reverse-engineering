#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/button/button.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/redarc_battery_sensor/battery_sensor.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <array>
#include <vector>

namespace esphome {
namespace redarc_manager {

class Manager30Component;

class VehicleInputTriggerSelect : public select::Select {
 public:
  void set_parent(Manager30Component *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;
  Manager30Component *parent_{nullptr};
};

class ChargingModeSelect : public select::Select {
 public:
  void set_parent(Manager30Component *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;
  Manager30Component *parent_{nullptr};
};

class Manager30SetClockButton : public button::Button {
 public:
  void set_parent(Manager30Component *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  Manager30Component *parent_{nullptr};
};

class Manager30ClearSolarHistoryButton : public button::Button {
 public:
  void set_parent(Manager30Component *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  Manager30Component *parent_{nullptr};
};

class Manager30Component : public Component {
 public:
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_host_address(uint8_t host_address) { this->host_address_ = host_address; }
  void set_filter_interval_ms(uint32_t ms) { this->filter_interval_ms_ = ms; }
  void set_battery_sensor(redarc_battery_sensor::BatterySensorComponent *b) { this->battery_sensor_ = b; }
  void set_time_source(time::RealTimeClock *t) { this->time_source_ = t; }
  void set_set_clock_button(Manager30SetClockButton *b) { this->set_clock_button_ = b; }
  void set_clear_solar_history_button(Manager30ClearSolarHistoryButton *b) { this->clear_solar_history_button_ = b; }
  void send_set_clock();
  void clear_solar_history();
  void set_output_current_sensor(sensor::Sensor *s) { this->output_current_sensor_ = s; }
  void set_battery_voltage_sensor(sensor::Sensor *s) { this->battery_voltage_sensor_ = s; }
  void set_source_device_current_sensor(sensor::Sensor *s) { this->source_device_current_sensor_ = s; }
  void set_solar_current_sensor(sensor::Sensor *s) { this->solar_current_sensor_ = s; }
  void set_solar_voltage_sensor(sensor::Sensor *s) { this->solar_voltage_sensor_ = s; }
  void set_solar_power_sensor(sensor::Sensor *s) { this->solar_power_sensor_ = s; }
  void set_solar_energy_sensor(sensor::Sensor *s) { this->solar_energy_sensor_ = s; }
  void set_solar_day_history_text_sensor(text_sensor::TextSensor *s) { this->solar_day_history_text_sensor_ = s; }
  void set_solar_history_poll_interval_ms(uint32_t ms) { this->solar_history_poll_interval_ms_ = ms; }
  void set_ac_input_voltage_sensor(sensor::Sensor *s) { this->ac_input_voltage_sensor_ = s; }
  void set_vehicle_input_current_sensor(sensor::Sensor *s) { this->vehicle_input_current_sensor_ = s; }
  void set_vehicle_input_voltage_sensor(sensor::Sensor *s) { this->vehicle_input_voltage_sensor_ = s; }
  void set_clock_date_text_sensor(text_sensor::TextSensor *s) { this->clock_date_text_sensor_ = s; }
  void set_clock_time_text_sensor(text_sensor::TextSensor *s) { this->clock_time_text_sensor_ = s; }
  void set_clock_datetime_text_sensor(text_sensor::TextSensor *s) { this->clock_datetime_text_sensor_ = s; }
  void set_charging_stage_text_sensor(text_sensor::TextSensor *s) { this->charging_stage_text_sensor_ = s; }
  void set_vehicle_input_trigger_select(VehicleInputTriggerSelect *s) { this->vehicle_input_trigger_select_ = s; }
  void set_charging_mode_select(ChargingModeSelect *s) { this->charging_mode_select_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  void send_vehicle_input_trigger(uint16_t raw_value);
  void send_charging_mode(uint8_t mode);

 protected:
  void publish_charging_mode_(uint8_t mode);
  void publish_charging_stage_(uint8_t stage);
  void publish_solar_daily_energy_(uint8_t day, uint16_t wh);
  void publish_solar_energy_total_();
  void publish_solar_day_history_();
  void send_solar_history_request_();

  uint8_t source_address_{0x01};
  uint8_t host_address_{0x20};
  uint32_t filter_interval_ms_{5000};
  uint32_t last_current_ms_{0};
  uint32_t last_solar_ms_{0};
  uint32_t last_ac_ms_{0};
  uint32_t last_charger_status_ms_{0};
  uint32_t last_manager_status_ms_{0};
  uint32_t last_charging_stage_ms_{0};
  uint32_t last_clock_ms_{0};
  bool charging_mode_status_seen_{false};
  redarc_battery_sensor::BatterySensorComponent *battery_sensor_{nullptr};
  sensor::Sensor *output_current_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *source_device_current_sensor_{nullptr};
  sensor::Sensor *solar_current_sensor_{nullptr};
  sensor::Sensor *solar_voltage_sensor_{nullptr};
  sensor::Sensor *solar_power_sensor_{nullptr};
  sensor::Sensor *solar_energy_sensor_{nullptr};
  std::array<uint16_t, 12> solar_daily_wh_{};
  std::array<bool, 12> solar_daily_known_{};
  text_sensor::TextSensor *solar_day_history_text_sensor_{nullptr};
  uint32_t solar_history_poll_interval_ms_{60000};
  uint32_t last_solar_history_poll_ms_{0};
  sensor::Sensor *ac_input_voltage_sensor_{nullptr};
  sensor::Sensor *vehicle_input_current_sensor_{nullptr};
  sensor::Sensor *vehicle_input_voltage_sensor_{nullptr};
  text_sensor::TextSensor *clock_date_text_sensor_{nullptr};
  text_sensor::TextSensor *clock_time_text_sensor_{nullptr};
  text_sensor::TextSensor *clock_datetime_text_sensor_{nullptr};
  text_sensor::TextSensor *charging_stage_text_sensor_{nullptr};
  VehicleInputTriggerSelect *vehicle_input_trigger_select_{nullptr};
  ChargingModeSelect *charging_mode_select_{nullptr};
  time::RealTimeClock *time_source_{nullptr};
  Manager30SetClockButton *set_clock_button_{nullptr};
  Manager30ClearSolarHistoryButton *clear_solar_history_button_{nullptr};
};

}  // namespace redarc_manager
}  // namespace esphome
