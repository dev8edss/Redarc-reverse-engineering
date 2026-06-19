#include "battery_sensor.h"

namespace esphome {
namespace redarc_battery_sensor {

static const char *const TAG = "redarc_battery_sensor";

void BatterySOCCalibrateButton::press_action() {
  if (this->parent_ == nullptr) return;
  this->parent_->send_config_setting(0x15, 100);
}

void BatteryConfigNumber::control(float value) {
  if (this->parent_ == nullptr) return;
  const uint16_t raw = (uint16_t) std::lround(value * this->raw_multiplier_);
  this->parent_->send_config_setting(this->command_, raw);
  this->publish_state(value);
}

void BatteryTypeSelect::control(size_t index) {
  if (this->parent_ == nullptr || index > 4) return;
  this->parent_->send_config_setting(0x11, (uint16_t) index);
  this->publish_state(index);
}

void BatterySensorComponent::setup() {
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void BatterySensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Battery Sensor SA 0x%02X", this->source_address_);
  LOG_SENSOR("  ", "Battery Type", this->battery_type_sensor_);
  LOG_SENSOR("  ", "Configured Capacity", this->configured_capacity_sensor_);
  LOG_SENSOR("  ", "Max Charge Current", this->max_charge_current_sensor_);
  LOG_SENSOR("  ", "Low SOC Alarm", this->low_soc_alarm_sensor_);
  LOG_SENSOR("  ", "Low Voltage Alarm", this->low_voltage_alarm_sensor_);
  LOG_SENSOR("  ", "Last SOC Calibration Target", this->last_soc_calibration_target_sensor_);
  LOG_SELECT("  ", "Battery Type", this->battery_type_select_);
  LOG_NUMBER("  ", "Configured Capacity", this->configured_capacity_number_);
  LOG_NUMBER("  ", "Max Charge Current", this->max_charge_current_number_);
  LOG_NUMBER("  ", "Low SOC Alarm", this->low_soc_alarm_number_);
  LOG_NUMBER("  ", "Low Voltage Alarm", this->low_voltage_alarm_number_);
  LOG_BUTTON("  ", "SOC Calibration", this->soc_calibration_button_);
}

void BatterySensorComponent::send_config_setting(uint8_t command, uint16_t raw_value) {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  const std::vector<uint8_t> data = {
      command, 0x00, 0xFF, 0xFF,
      (uint8_t) (raw_value & 0xFF), (uint8_t) (raw_value >> 8),
      0x00, 0x00};
  bus->send_data(0x0F00FF20UL, true, data);
  ESP_LOGD(TAG, "Sent battery config command 0x%02X value %u", command, raw_value);
}

void BatterySensorComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  if ((can_id == 0x0F0008FAUL && data[2] == 0x03) ||
      (can_id == 0x0F00FF20UL && data[2] == 0xFF && data[3] == 0xFF)) {
    const uint16_t raw = redarc_common::u16_le(data, 4);
    switch (data[0]) {
      case 0x12:
        if (this->configured_capacity_sensor_ != nullptr) this->configured_capacity_sensor_->publish_state((float) raw);
        if (this->configured_capacity_number_ != nullptr) this->configured_capacity_number_->publish_state((float) raw);
        break;
      case 0x11:
        if (this->battery_type_sensor_ != nullptr) this->battery_type_sensor_->publish_state((float) raw);
        if (this->battery_type_select_ != nullptr && raw <= 4) this->battery_type_select_->publish_state((size_t) raw);
        break;
      case 0x14:
        if (this->max_charge_current_sensor_ != nullptr) this->max_charge_current_sensor_->publish_state((float) raw);
        if (this->max_charge_current_number_ != nullptr) this->max_charge_current_number_->publish_state((float) raw);
        break;
      case 0x15:
        if (this->last_soc_calibration_target_sensor_ != nullptr)
          this->last_soc_calibration_target_sensor_->publish_state((float) raw);
        break;
      case 0x41:
        if (this->low_soc_alarm_sensor_ != nullptr) this->low_soc_alarm_sensor_->publish_state((float) raw);
        if (this->low_soc_alarm_number_ != nullptr) this->low_soc_alarm_number_->publish_state((float) raw);
        break;
      case 0x42:
        if (this->low_voltage_alarm_sensor_ != nullptr) this->low_voltage_alarm_sensor_->publish_state((float) raw * 0.001f);
        if (this->low_voltage_alarm_number_ != nullptr) this->low_voltage_alarm_number_->publish_state((float) raw * 0.001f);
        break;
      default:
        break;
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F100UL, this->source_address_)) {
    const uint32_t now = millis();
    if (now - this->last_capacity_ms_ >= this->filter_interval_ms_) {
      this->last_capacity_ms_ = now;
      if (this->battery_type_sensor_ != nullptr && data[0] <= 0x04) {
        this->battery_type_sensor_->publish_state((float) data[0]);
      }
      if (this->battery_type_select_ != nullptr && data[0] <= 0x04) {
        this->battery_type_select_->publish_state((size_t) data[0]);
      }
      if (this->configured_capacity_sensor_ != nullptr) {
        this->configured_capacity_sensor_->publish_state((float) redarc_common::u16_le(data, 1));
      }
      if (this->configured_capacity_number_ != nullptr) {
        this->configured_capacity_number_->publish_state((float) redarc_common::u16_le(data, 1));
      }
      if (this->max_charge_current_sensor_ != nullptr && data[4] != 0xFF) {
        this->max_charge_current_sensor_->publish_state((float) data[4]);
      }
      if (this->max_charge_current_number_ != nullptr && data[4] != 0xFF) {
        this->max_charge_current_number_->publish_state((float) data[4]);
      }
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F10AUL, this->source_address_)) {
    const uint32_t now = millis();
    if (now - this->last_alarm_report_ms_ >= this->filter_interval_ms_) {
      this->last_alarm_report_ms_ = now;
      if (this->low_soc_alarm_sensor_ != nullptr && data[1] != 0xFF) {
        this->low_soc_alarm_sensor_->publish_state((float) data[1]);
      }
      if (this->low_soc_alarm_number_ != nullptr && data[1] != 0xFF) {
        this->low_soc_alarm_number_->publish_state((float) data[1]);
      }
      const uint16_t raw_low_voltage = redarc_common::u16_le(data, 2);
      if (this->low_voltage_alarm_sensor_ != nullptr && raw_low_voltage != 0xFFFF) {
        this->low_voltage_alarm_sensor_->publish_state((float) raw_low_voltage * 0.001f);
      }
      if (this->low_voltage_alarm_number_ != nullptr && raw_low_voltage != 0xFFFF) {
        this->low_voltage_alarm_number_->publish_state((float) raw_low_voltage * 0.001f);
      }
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F102UL, this->source_address_)) {
    const uint32_t raw = redarc_common::u32_le(data, 0);
    const float amps = redarc_common::current_32_centered(raw);
    this->current_a_ = amps;
    const uint32_t now = millis();
    if (now - this->last_current_ms_ >= this->filter_interval_ms_) {
      this->last_current_ms_ = now;
      if (this->current_sensor_ != nullptr) this->current_sensor_->publish_state(amps);
      if (this->current_raw_sensor_ != nullptr) this->current_raw_sensor_->publish_state((float) raw);
      if (this->voltage_sensor_ != nullptr) this->voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4) * 0.001f);
      if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state((float) data[6] - 60.0f);
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F104UL, this->source_address_)) {
    const uint32_t now = millis();
    if (now - this->last_soc_ms_ >= this->filter_interval_ms_) {
      this->last_soc_ms_ = now;
      if (this->soc_sensor_ != nullptr) this->soc_sensor_->publish_state((float) data[0]);
    }
    return;
  }
}

}  // namespace redarc_battery_sensor
}  // namespace esphome
