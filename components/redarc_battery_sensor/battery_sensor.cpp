#include "battery_sensor.h"

#include <algorithm>
#include <cstdio>

namespace esphome {
namespace redarc_battery_sensor {

static const char *const TAG = "redarc_battery_sensor";
static const uint32_t SOC_HISTORY_REQUEST_IDS[] = {
    0x0F03FFFAUL, 0x0F03FFFAUL, 0x0FFCD0FAUL, 0x0FFCD2FAUL, 0x0FFCD4FAUL};
static const uint8_t SOC_HISTORY_REQUEST_DATA[][8] = {
    {0x07, 0xF2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x05, 0xF2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static const uint8_t SOC_HISTORY_REQUEST_COUNT = sizeof(SOC_HISTORY_REQUEST_IDS) / sizeof(SOC_HISTORY_REQUEST_IDS[0]);
static const uint32_t SOC_HISTORY_REQUEST_SPACING_MS = 100;

void BatterySOCCalibrateButton::press_action() {
  if (this->parent_ == nullptr) return;
  this->parent_->send_soc_calibration_command(100);
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
  this->soc_hourly_history_.fill(0xFF);
  this->soc_daily_low_history_.fill(0xFF);
  this->soc_daily_high_history_.fill(0xFF);
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void BatterySensorComponent::loop() {
  // Active SOC history polling is disabled while Manager30 reset behavior is under investigation.
}

void BatterySensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Battery Sensor SA 0x%02X", this->source_address_);
  ESP_LOGCONFIG(TAG, "  SOC history poll interval: %u ms", (unsigned) this->soc_history_poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  SOC history active requests: disabled");
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
  LOG_TEXT_SENSOR("  ", "SOC Hourly History", this->soc_hourly_history_text_sensor_);
  LOG_TEXT_SENSOR("  ", "SOC Daily Low History", this->soc_daily_low_history_text_sensor_);
  LOG_TEXT_SENSOR("  ", "SOC Daily High History", this->soc_daily_high_history_text_sensor_);
  LOG_TEXT_SENSOR("  ", "SOC Daily Range History", this->soc_daily_range_history_text_sensor_);
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

void BatterySensorComponent::send_soc_calibration_command(uint8_t target_percent) {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  const std::vector<uint8_t> data = {
      0x15, 0x00, 0x03, 0x00,
      target_percent, 0x00,
      0x00, 0x00};
  const uint32_t can_id = 0x0F0000FAUL | ((uint32_t) this->source_address_ << 8);
  bus->send_data(can_id, true, data);
  ESP_LOGD(TAG, "Sent SOC calibration command target %u%% to SA 0x%02X", target_percent, this->source_address_);
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

  if (redarc_common::rvc_matches(can_id, 0x1FCD0UL, this->source_address_) ||
      redarc_common::rvc_matches(can_id, 0x1FCD2UL, this->source_address_) ||
      redarc_common::rvc_matches(can_id, 0x1FCD4UL, this->source_address_)) {
    this->handle_soc_history_page_(redarc_common::rvc_dgn(can_id), data);
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

void BatterySensorComponent::request_soc_history_() {
  this->pending_soc_history_request_index_ = 0;
  this->last_soc_history_request_ms_ = 0;
  ESP_LOGD(TAG, "Scheduled battery SOC history requests");
}

bool BatterySensorComponent::send_pending_soc_history_request_(uint32_t now) {
  if (this->pending_soc_history_request_index_ >= SOC_HISTORY_REQUEST_COUNT) return false;
  if (this->last_soc_history_request_ms_ != 0 &&
      now - this->last_soc_history_request_ms_ < SOC_HISTORY_REQUEST_SPACING_MS)
    return true;

  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) {
    this->pending_soc_history_request_index_ = 0xFF;
    return false;
  }

  const uint8_t request_index = this->pending_soc_history_request_index_;
  const std::vector<uint8_t> request(
      SOC_HISTORY_REQUEST_DATA[request_index],
      SOC_HISTORY_REQUEST_DATA[request_index] + sizeof(SOC_HISTORY_REQUEST_DATA[request_index]));
  bus->send_data(SOC_HISTORY_REQUEST_IDS[request_index], true, request);
  this->pending_soc_history_request_index_++;
  this->last_soc_history_request_ms_ = now;
  ESP_LOGD(TAG, "Sent battery SOC history request step %u", (unsigned) request_index);
  return this->pending_soc_history_request_index_ < SOC_HISTORY_REQUEST_COUNT;
}

void BatterySensorComponent::handle_soc_history_page_(uint32_t dgn, const std::vector<uint8_t> &data) {
  const uint8_t page = data[0];
  if (page > 4) return;
  const uint8_t base = page * 7U;
  for (uint8_t i = 1; i < 8; i++) {
    const uint8_t index = base + i - 1U;
    const uint8_t value = data[i];
    if (dgn == 0x1FCD0UL) {
      if (index < this->soc_hourly_history_.size()) this->soc_hourly_history_[index] = value;
    } else if (dgn == 0x1FCD2UL) {
      if (index < this->soc_daily_low_history_.size()) this->soc_daily_low_history_[index] = value;
    } else if (dgn == 0x1FCD4UL) {
      if (index < this->soc_daily_high_history_.size()) this->soc_daily_high_history_[index] = value;
    }
  }
  if (dgn == 0x1FCD0UL) {
    this->publish_soc_hourly_history_();
  } else {
    this->publish_soc_daily_history_();
  }
}

void BatterySensorComponent::publish_soc_hourly_history_() {
  if (this->soc_hourly_history_text_sensor_ == nullptr) return;
  char buffer[128];
  size_t used = 0;
  bool first = true;
  buffer[0] = '\0';
  for (uint8_t value : this->soc_hourly_history_) {
    if (value == 0xFF) continue;
    this->append_csv_value_(buffer, sizeof(buffer), used, value, first);
  }
  this->soc_hourly_history_text_sensor_->publish_state(buffer);
}

void BatterySensorComponent::publish_soc_daily_history_() {
  char low_buffer[160];
  char high_buffer[160];
  char range_buffer[240];
  size_t low_used = 0;
  size_t high_used = 0;
  size_t range_used = 0;
  bool low_first = true;
  bool high_first = true;
  bool range_first = true;
  low_buffer[0] = '\0';
  high_buffer[0] = '\0';
  range_buffer[0] = '\0';

  const size_t day_count = std::min<size_t>(30, this->soc_daily_low_history_.size());
  for (size_t i = 0; i < day_count; i++) {
    const uint8_t low = this->soc_daily_low_history_[i];
    const uint8_t high = this->soc_daily_high_history_[i];
    if (low != 0xFF) this->append_csv_value_(low_buffer, sizeof(low_buffer), low_used, low, low_first);
    if (high != 0xFF) this->append_csv_value_(high_buffer, sizeof(high_buffer), high_used, high, high_first);
    if (low != 0xFF || high != 0xFF) {
      this->append_csv_range_(range_buffer, sizeof(range_buffer), range_used, low, high, range_first);
    }
  }

  if (this->soc_daily_low_history_text_sensor_ != nullptr)
    this->soc_daily_low_history_text_sensor_->publish_state(low_buffer);
  if (this->soc_daily_high_history_text_sensor_ != nullptr)
    this->soc_daily_high_history_text_sensor_->publish_state(high_buffer);
  if (this->soc_daily_range_history_text_sensor_ != nullptr)
    this->soc_daily_range_history_text_sensor_->publish_state(range_buffer);
}

void BatterySensorComponent::append_csv_value_(char *buffer, size_t buffer_size, size_t &used, uint8_t value, bool &first) {
  if (used >= buffer_size) return;
  const int written = std::snprintf(buffer + used, buffer_size - used, first ? "%u" : ",%u", (unsigned) value);
  if (written <= 0) return;
  used += (size_t) written;
  if (used >= buffer_size) used = buffer_size - 1;
  first = false;
}

void BatterySensorComponent::append_csv_range_(char *buffer, size_t buffer_size, size_t &used, uint8_t low, uint8_t high, bool &first) {
  if (used >= buffer_size) return;
  const char *separator = first ? "" : ",";
  int written;
  if (low == 0xFF) {
    written = std::snprintf(buffer + used, buffer_size - used, "%s255-%u", separator, (unsigned) high);
  } else if (high == 0xFF) {
    written = std::snprintf(buffer + used, buffer_size - used, "%s%u-255", separator, (unsigned) low);
  } else {
    written = std::snprintf(buffer + used, buffer_size - used, "%s%u-%u", separator, (unsigned) low, (unsigned) high);
  }
  if (written <= 0) return;
  used += (size_t) written;
  if (used >= buffer_size) used = buffer_size - 1;
  first = false;
}

}  // namespace redarc_battery_sensor
}  // namespace esphome
