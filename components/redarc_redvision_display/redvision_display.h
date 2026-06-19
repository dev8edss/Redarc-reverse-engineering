#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <vector>

namespace esphome {
namespace redarc_redvision_display {

class RedvisionDisplayComponent : public Component {
 public:
  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_filter_interval_ms(uint32_t ms) { this->filter_interval_ms_ = ms; }
  void set_battery_current_display_sensor(sensor::Sensor *s) { this->battery_current_display_sensor_ = s; }
  void set_device_current_display_sensor(sensor::Sensor *s) { this->device_current_display_sensor_ = s; }
  void set_manager_output_current_display_sensor(sensor::Sensor *s) { this->manager_output_current_display_sensor_ = s; }
  void set_battery_current_display_raw_sensor(sensor::Sensor *s) { this->battery_current_display_raw_sensor_ = s; }
  void set_device_current_display_raw_sensor(sensor::Sensor *s) { this->device_current_display_raw_sensor_ = s; }
  void set_manager_output_current_display_raw_sensor(sensor::Sensor *s) { this->manager_output_current_display_raw_sensor_ = s; }
  void setup() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);

 protected:
  uint8_t source_address_{0x20};
  uint32_t filter_interval_ms_{5000};
  uint32_t last_display_ms_{0};
  sensor::Sensor *battery_current_display_sensor_{nullptr};
  sensor::Sensor *device_current_display_sensor_{nullptr};
  sensor::Sensor *manager_output_current_display_sensor_{nullptr};
  sensor::Sensor *battery_current_display_raw_sensor_{nullptr};
  sensor::Sensor *device_current_display_raw_sensor_{nullptr};
  sensor::Sensor *manager_output_current_display_raw_sensor_{nullptr};
};

}  // namespace redarc_redvision_display
}  // namespace esphome
