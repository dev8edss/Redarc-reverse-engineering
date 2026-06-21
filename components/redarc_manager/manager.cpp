#include "manager.h"
#include <cstdio>

namespace esphome {
namespace redarc_manager {

static const char *const TAG = "redarc_manager";
static const uint32_t SOLAR_HISTORY_INITIAL_OFFSET_MS = 30000;
static const uint32_t HISTORY_PREAMBLE_DELAY_MS = 3000;

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
  this->publish_can_status_(false, "Normal");
}

void Manager30Component::loop() {
  const uint32_t now = millis();
  if (this->solar_history_poll_interval_ms_ == 0) return;
  if (this->last_solar_history_poll_ms_ == 0) {
    this->last_solar_history_poll_ms_ = now + SOLAR_HISTORY_INITIAL_OFFSET_MS - this->solar_history_poll_interval_ms_;
    return;
  }

  if (this->pending_solar_history_poll_) {
    if (now - this->pending_solar_history_preamble_ms_ < HISTORY_PREAMBLE_DELAY_MS) return;
    this->pending_solar_history_poll_ = false;
    this->last_solar_history_poll_ms_ = now;
    this->request_solar_history_();
    return;
  }

  const uint32_t lead_ms =
      this->solar_history_poll_interval_ms_ > HISTORY_PREAMBLE_DELAY_MS ? HISTORY_PREAMBLE_DELAY_MS : 0;
  if (now - this->last_solar_history_poll_ms_ >= this->solar_history_poll_interval_ms_ - lead_ms) {
    this->send_history_preamble_();
    this->pending_solar_history_preamble_ms_ = now;
    this->pending_solar_history_poll_ = true;
  }
}

void Manager30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Manager30 SA 0x%02X", this->source_address_);
  ESP_LOGCONFIG(TAG, "  Solar history poll interval: %u ms", (unsigned) this->solar_history_poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Solar history initial offset: %u ms", (unsigned) SOLAR_HISTORY_INITIAL_OFFSET_MS);
  ESP_LOGCONFIG(TAG, "  Solar history active requests: %s", this->solar_history_poll_interval_ms_ == 0 ? "disabled" : "display DGN");
  LOG_SENSOR("  ", "Vehicle Input Trigger", this->vehicle_input_trigger_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Date", this->clock_date_text_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Time", this->clock_time_text_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Date Time", this->clock_datetime_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Charging Stage", this->charging_stage_text_sensor_);
  LOG_BINARY_SENSOR("  ", "CAN Status Abnormal", this->can_status_abnormal_sensor_);
  LOG_TEXT_SENSOR("  ", "CAN Status", this->can_status_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Solar Day -1..-5 History", this->solar_day_1_5_history_text_sensor_);
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
  if (data.size() < 8) {
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Short CAN frame 0x%08X dlc=%u", (unsigned) can_id, (unsigned) data.size());
    this->publish_can_status_(true, msg);
    return;
  }

  const uint32_t now = millis();
  this->inspect_status_heartbeat_(can_id, data);

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
    (void) flags;
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

  // Manager30 solar generation history. A request on 0x1BFCD620 is followed by
  // pages on 0x03FCD601. D1 is the page index; D2-D3, D4-D5 and D6-D7 are
  // little-endian Wh buckets. Page 0 starts with today, then previous days.
  if (redarc_common::rvc_matches(can_id, 0x1FCD6UL, this->source_address_)) {
    const uint8_t page = data[0];
    if (page > 2) return;
    const uint8_t base_day = page * 3U;
    for (uint8_t slot = 0; slot < 3; slot++) {
      const uint8_t day = base_day + slot;
      if (day >= 8) break;
      const uint8_t offset = 1U + slot * 2U;
      this->publish_solar_daily_energy_(day, redarc_common::u16_le(data, offset));
    }
    this->last_solar_energy_ms_ = now;
    this->publish_solar_energy_total_();
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

void Manager30Component::publish_solar_daily_energy_(uint8_t day, uint16_t wh) {
  if (day >= 8) return;
  this->solar_daily_wh_[day] = wh;
  this->solar_daily_known_[day] = true;
  if (this->solar_daily_energy_sensors_[day] != nullptr)
    this->solar_daily_energy_sensors_[day]->publish_state((float) wh);
}

void Manager30Component::publish_solar_energy_total_() {
  if (this->solar_energy_sensor_ == nullptr) return;
  uint32_t total = 0;
  bool any = false;
  for (uint8_t i = 0; i < 8; i++) {
    if (!this->solar_daily_known_[i]) continue;
    total += this->solar_daily_wh_[i];
    any = true;
  }
  if (any) this->solar_energy_sensor_->publish_state((float) total);
  if (any) this->publish_solar_day_1_5_history_();
}

void Manager30Component::publish_solar_day_1_5_history_() {
  if (this->solar_day_1_5_history_text_sensor_ == nullptr) return;

  char buffer[48];
  size_t used = 0;
  buffer[0] = '\0';
  for (uint8_t day = 1; day <= 5; day++) {
    const unsigned value = this->solar_daily_known_[day] ? (unsigned) this->solar_daily_wh_[day] : 255U;
    const int written = std::snprintf(buffer + used, sizeof(buffer) - used, day == 1 ? "%u" : ",%u", value);
    if (written <= 0) break;
    used += (size_t) written;
    if (used >= sizeof(buffer)) {
      buffer[sizeof(buffer) - 1] = '\0';
      break;
    }
  }
  this->solar_day_1_5_history_text_sensor_->publish_state(buffer);
}

void Manager30Component::inspect_status_heartbeat_(uint32_t can_id, const std::vector<uint8_t> &data) {
  const uint32_t dgn = redarc_common::rvc_dgn(can_id);
  const uint8_t sa = redarc_common::rvc_source_address(can_id);
  char msg[96];

  if (dgn == 0x1F200UL && sa == this->source_address_) {
    if (!this->is_valid_charging_stage_(data[0])) {
      std::snprintf(msg, sizeof(msg), "Manager30 unknown charging stage 0x%02X", data[0]);
      this->publish_can_status_(true, msg);
    } else {
      this->publish_can_status_(false, "Normal");
    }
    return;
  }

  if (dgn == 0x1F206UL && sa == this->source_address_) {
    if (data[7] != 0xFF && !this->is_valid_vehicle_trigger_(data[7])) {
      std::snprintf(msg, sizeof(msg), "Manager30 invalid vehicle trigger 0x%02X", data[7]);
      this->publish_can_status_(true, msg);
    } else {
      this->publish_can_status_(false, "Normal");
    }
    return;
  }

  if (dgn == 0x1FD00UL && (sa == 0x24 || sa == 0x30)) {
    for (uint8_t i = 1; i < 8; i++) {
      const uint8_t value = data[i];
      if (value == 0x00 || value == 0x01 || value == 0xF8 || value == 0xFF) continue;
      std::snprintf(msg, sizeof(msg), "SA 0x%02X status page 0x%02X unexpected byte D%u=0x%02X",
                    sa, data[0], (unsigned) (i + 1), value);
      this->publish_can_status_(true, msg);
      return;
    }
    this->publish_can_status_(false, "Normal");
  }
}

void Manager30Component::publish_can_status_(bool abnormal, const char *message) {
  if (this->can_status_published_ && abnormal == this->can_status_abnormal_) {
    if (abnormal && this->can_status_text_sensor_ != nullptr) this->can_status_text_sensor_->publish_state(message);
    return;
  }
  this->can_status_published_ = true;
  this->can_status_abnormal_ = abnormal;
  if (this->can_status_abnormal_sensor_ != nullptr) this->can_status_abnormal_sensor_->publish_state(abnormal);
  if (this->can_status_text_sensor_ != nullptr) this->can_status_text_sensor_->publish_state(message);
  if (abnormal) {
    ESP_LOGW(TAG, "CAN status abnormal: %s", message);
  } else {
    ESP_LOGI(TAG, "CAN status normal");
  }
}

void Manager30Component::request_solar_history_() {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  bus->send_data(0x1BFCD620UL, true, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  ESP_LOGD(TAG, "Requested Manager30 solar generation history");
}

void Manager30Component::send_history_preamble_() {
  auto *bus = redarc_common::RedarcCanDispatcher::instance().canbus();
  if (bus == nullptr) return;
  bus->send_data(0x0F00FF20UL, true, {0x4D, 0x00, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00});
  ESP_LOGD(TAG, "Sent history polling preamble before solar history request");
}

bool Manager30Component::is_valid_charging_stage_(uint8_t stage) const {
  switch (stage) {
    case 0x00:
    case 0x01:
    case 0x21:
    case 0x30:
    case 0x31:
    case 0x40:
    case 0x41:
    case 0x70:
    case 0x71:
    case 0x80:
    case 0x81:
      return true;
    default:
      return false;
  }
}

bool Manager30Component::is_valid_vehicle_trigger_(uint8_t trigger) const {
  return trigger <= 3 || trigger == 5;
}

void Manager30Component::publish_charging_mode_(uint8_t mode) {
  if (mode > 1) return;
  if (this->charging_mode_select_ != nullptr) this->charging_mode_select_->publish_state((size_t) mode);
}

void Manager30Component::publish_charging_stage_(uint8_t stage) {
  if (this->charging_stage_text_sensor_ == nullptr) return;

  const char *name = nullptr;
  switch (stage) {
    case 0x00:
    case 0x01:
      name = "Not Charging";
      break;
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
