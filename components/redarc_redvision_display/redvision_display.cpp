#include "redvision_display.h"

namespace esphome {
namespace redarc_redvision_display {

static const char *const TAG = "redarc_redvision_display";

void RedvisionDisplayComponent::setup() {
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });
}

void RedvisionDisplayComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Redvision display SA 0x%02X", this->source_address_);
}

void RedvisionDisplayComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  can_id = redarc_common::rvc_id(can_id);
  if (data.size() < 8) return;

  if (redarc_common::rvc_source_address(can_id) != this->source_address_) return;

  const uint32_t now = millis();
  const bool ok = (now - this->last_display_ms_) >= this->filter_interval_ms_;

  switch (redarc_common::rvc_dgn(can_id)) {
  case 0x1F280UL: {
    if (!ok) return;
    this->last_display_ms_ = now;
    const uint16_t raw_batt = redarc_common::u16_le(data, 0);
    const uint16_t raw_load = redarc_common::u16_le(data, 4);
    if (this->battery_current_display_sensor_ != nullptr) this->battery_current_display_sensor_->publish_state(redarc_common::current_display_16_centered(raw_batt));
    if (this->device_current_display_sensor_ != nullptr) this->device_current_display_sensor_->publish_state(redarc_common::current_display_16_centered(raw_load));
    return;
  }

  case 0x1F282UL: {
    if (!ok) return;
    this->last_display_ms_ = now;
    const uint16_t raw_mgr = redarc_common::u16_le(data, 6);
    if (this->manager_output_current_display_sensor_ != nullptr) this->manager_output_current_display_sensor_->publish_state(redarc_common::current_display_16_centered(raw_mgr));
    return;
  }

  default:
    return;
  }
}

}  // namespace redarc_redvision_display
}  // namespace esphome
