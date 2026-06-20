#include "manager.h"
#include <cstdio>

namespace esphome {
namespace redarc_manager {

static const char *const TAG = "redarc_manager";

void VehicleInputTriggerSelect::control(size_t index) {
  static const uint16_t VALUES[] = {0, 1, 2, 3, 5};
  if (this->parent_ == nullptr || index >= 5) return;
  this->parent_->send_vehicle_input_trigger(VALUES[index]);
  this->publish_state(index);
}

void ChargingModeSelect::control(size_t index) {
  if (this->parent_ == nullptr || index >= 2) return;
  this->parent_->send_charging_mode((uint8_t) index);
  this->publish_state(index);
}

void Manager30Component::setup() {
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void Manager30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Manager30 SA 0x%02X", this->source_address_);
  LOG_SENSOR("  ", "Vehicle Input Trigger", this->vehicle_input_trigger_sensor_);
  LOG_SENSOR("  ", "Clock Flags Raw", this->clock_flags_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Date", this->clock_date_text_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Time", this->clock_time_text_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Date Time", this->clock_datetime_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Charging Stage", this->charging_stage_text_sensor_);
  LOG_SELECT("  ", "Vehicle Input Trigger", this->vehicle_input_trigger_select_);
  LOG_SELECT("  ", "Charging Mode", this->charging_mode_select_);
}

void Manager30Component::send_vehicle_input_trigger(uint16_t raw_value) {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  const std::vector<uint8_t> data = {
      0x68, 0x00, 0xFF, 0xFF,
      (uint8_t) (raw_value & 0xFF), (uint8_t) (raw_value >> 8),
      0x00, 0x00};
  bus->send_data(0x0F00FF20UL, true, data);
  ESP_LOGD(TAG, "Sent vehicle input trigger %u", raw_value);
}

void Manager30Component::send_charging_mode(uint8_t mode) {
  if (mode > 1) return;
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  const std::vector<uint8_t> data = {0x43, 0x00, 0xFF, 0xFF, mode, 0x00, 0x00, 0x00};
  bus->send_data(0x0F00FF20UL, true, data);
  ESP_LOGD(TAG, "Sent charging mode %s", mode == 0 ? "Touring" : "Storage");
}

void Manager30Component::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  const uint32_t now = millis();

  if (can_id == 0x0F0001FAUL && data[0] == 0x68 && data[2] == 0x03) {
    const uint16_t raw = redarc_common::u16_le(data, 4);
    if (this->vehicle_input_trigger_sensor_ != nullptr) this->vehicle_input_trigger_sensor_->publish_state((float) raw);
    if (this->vehicle_input_trigger_select_ != nullptr) {
      if (raw <= 3) this->vehicle_input_trigger_select_->publish_state((size_t) raw);
      else if (raw == 5) this->vehicle_input_trigger_select_->publish_state((size_t) 4);
    }
    return;
  }

  if (can_id == 0x0F00FF20UL && data[0] == 0x68 && data[2] == 0xFF && data[3] == 0xFF) {
    const uint16_t raw = redarc_common::u16_le(data, 4);
    if (this->vehicle_input_trigger_sensor_ != nullptr) this->vehicle_input_trigger_sensor_->publish_state((float) raw);
    if (this->vehicle_input_trigger_select_ != nullptr) {
      if (raw <= 3) this->vehicle_input_trigger_select_->publish_state((size_t) raw);
      else if (raw == 5) this->vehicle_input_trigger_select_->publish_state((size_t) 4);
    }
    return;
  }

  if (can_id == 0x0F00FF20UL && data[0] == 0x43 && data[1] == 0x00 && data[2] == 0xFF && data[3] == 0xFF) {
    this->publish_charging_mode_(data[4] & 0x01U);
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F108UL, this->source_address_)) {
    if (now - this->last_manager_status_ms_ < this->filter_interval_ms_) return;
    this->last_manager_status_ms_ = now;
    this->charging_mode_status_seen_ = true;
    this->publish_charging_mode_(data[0] & 0x01U);
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F200UL, this->source_address_)) {
    if (now - this->last_charging_stage_ms_ < this->filter_interval_ms_) return;
    this->last_charging_stage_ms_ = now;
    this->publish_charging_stage_(data[0]);
    if (!this->charging_mode_status_seen_) this->publish_charging_mode_(data[0] & 0x01U);
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F206UL, this->source_address_)) {
    if (now - this->last_charger_status_ms_ < this->filter_interval_ms_) return;
    this->last_charger_status_ms_ = now;
    if (this->vehicle_input_trigger_sensor_ != nullptr && data[7] != 0xFF)
      this->vehicle_input_trigger_sensor_->publish_state((float) data[7]);
    if (this->vehicle_input_trigger_select_ != nullptr && data[7] != 0xFF) {
      if (data[7] <= 3) this->vehicle_input_trigger_select_->publish_state((size_t) data[7]);
      else if (data[7] == 5) this->vehicle_input_trigger_select_->publish_state((size_t) 4);
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F304UL, this->source_address_)) {
    if (now - this->last_clock_ms_ < this->filter_interval_ms_) return;
    this->last_clock_ms_ = now;
    const uint8_t raw_day_flags = data[0];
    const uint8_t day = (raw_day_flags >> 3) + 1U;
    const uint8_t flags = raw_day_flags & 0x07U;
    const uint8_t month = data[1];
    const uint16_t year = redarc_common::u16_le(data, 2);
    const uint8_t hour = data[4];
    const uint8_t minute = data[5];
    const uint8_t second = data[6];
    if (this->clock_flags_sensor_ != nullptr) this->clock_flags_sensor_->publish_state((float) flags);
    if (year >= 2000 && year <= 2100 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
        hour <= 23 && minute <= 59 && second <= 59) {
      char date[11];
      char time[9];
      char datetime[20];
      std::snprintf(date, sizeof(date), "%04u-%02u-%02u", (unsigned) year, (unsigned) month, (unsigned) day);
      std::snprintf(time, sizeof(time), "%02u:%02u:%02u", (unsigned) hour, (unsigned) minute, (unsigned) second);
      std::snprintf(datetime, sizeof(datetime), "%04u-%02u-%02u %02u:%02u:%02u",
                    (unsigned) year, (unsigned) month, (unsigned) day,
                    (unsigned) hour, (unsigned) minute, (unsigned) second);
      if (this->clock_date_text_sensor_ != nullptr) this->clock_date_text_sensor_->publish_state(date);
      if (this->clock_time_text_sensor_ != nullptr) this->clock_time_text_sensor_->publish_state(time);
      if (this->clock_datetime_text_sensor_ != nullptr) this->clock_datetime_text_sensor_->publish_state(datetime);
    } else {
      ESP_LOGD(TAG, "Ignored invalid CAN clock payload: %02X %02X %02X %02X %02X %02X %02X %02X",
               data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F20AUL, this->source_address_)) {
    if (now - this->last_current_ms_ < this->filter_interval_ms_) return;
    this->last_current_ms_ = now;
    const uint32_t raw = redarc_common::u32_le(data, 0);
    const float amps = redarc_common::current_32_centered(raw);
    if (this->output_current_sensor_ != nullptr) this->output_current_sensor_->publish_state(amps);
    if (this->output_current_raw_sensor_ != nullptr) this->output_current_raw_sensor_->publish_state((float) raw);
    if (this->battery_voltage_sensor_ != nullptr) this->battery_voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4) * 0.001f);
    if (this->source_device_current_sensor_ != nullptr && this->battery_sensor_ != nullptr && this->battery_sensor_->has_current()) {
      this->source_device_current_sensor_->publish_state(amps - this->battery_sensor_->current_a());
    }
    return;
  }

  if (redarc_common::rvc_matches(can_id, 0x1F208UL, this->source_address_)) {
    if (now - this->last_solar_ms_ < this->filter_interval_ms_) return;
    this->last_solar_ms_ = now;
    const uint32_t raw_current = redarc_common::u32_le(data, 0);
    const uint16_t raw_voltage = redarc_common::u16_le(data, 4);
    const float solar_a = redarc_common::current_32_centered(raw_current);
    const float solar_v = (float) raw_voltage * 0.001f;
    if (this->solar_current_sensor_ != nullptr) this->solar_current_sensor_->publish_state(solar_a);
    if (this->solar_voltage_sensor_ != nullptr) this->solar_voltage_sensor_->publish_state(solar_v);
    if (this->solar_power_sensor_ != nullptr) this->solar_power_sensor_->publish_state(solar_a * solar_v);
    return;
  }

  // Manager30 solar energy/yield counter. Logs show 0x03FCD601 with D1=0x00
  // increments D2-D5 little-endian in Wh; example: 14 -> 19 Wh while solar
  // power was about 87 W. D1=0x01/0x02 companion frames were observed but
  // remain unknown and are ignored.
  if (redarc_common::rvc_matches(can_id, 0x1FCD6UL, this->source_address_)) {
    if (data[0] != 0x00) return;
    if (now - this->last_solar_energy_ms_ < this->filter_interval_ms_) return;
    this->last_solar_energy_ms_ = now;
    if (this->solar_energy_sensor_ != nullptr) {
      const uint32_t wh = redarc_common::u32_le(data, 1);
      this->solar_energy_sensor_->publish_state((float) wh);
    }
    return;
  }

  // DBC confirmed PGN_03F204_Manager30_AC_DC_Voltage_Candidates:
  // AC_Input_Voltage is D5-D6 little-endian, scale 1 V/count.
  if (redarc_common::rvc_matches(can_id, 0x1F204UL, this->source_address_)) {
    if (now - this->last_ac_ms_ < this->filter_interval_ms_) return;
    this->last_ac_ms_ = now;
    if (this->ac_input_voltage_sensor_ != nullptr) {
      this->ac_input_voltage_sensor_->publish_state((float) redarc_common::u16_le(data, 4));
    }
    return;
  }
}

void Manager30Component::publish_charging_mode_(uint8_t mode) {
  if (mode > 1) return;
  if (this->charging_mode_select_ != nullptr) this->charging_mode_select_->publish_state((size_t) mode);
}

void Manager30Component::publish_charging_stage_(uint8_t stage) {
  if (this->charging_stage_text_sensor_ == nullptr) return;

  const char *name = nullptr;
  switch (stage) {
    case 0x21:
      name = "Soft-start";
      break;
    case 0x30:
    case 0x31:
      name = "Boost";
      break;
    case 0x40:
    case 0x41:
      name = "Absorption";
      break;
    case 0x70:
    case 0x71:
      name = "Float";
      break;
    case 0x80:
    case 0x81:
      name = "Maintenance";
      break;
    default:
      break;
  }

  if (name != nullptr) {
    this->charging_stage_text_sensor_->publish_state(name);
  } else {
    char unknown[13];
    std::snprintf(unknown, sizeof(unknown), "Unknown 0x%02X", stage);
    this->charging_stage_text_sensor_->publish_state(unknown);
  }
}

}  // namespace redarc_manager
}  // namespace esphome
