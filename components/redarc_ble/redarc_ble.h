#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace redarc_ble {

class RedarcBLEComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_device_name(const std::string &device_name) { this->device_name_ = device_name; }
  void set_update_interval_ms(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }

 protected:
  std::string build_demo_sensor_json_();

  std::string device_name_{"Redarc Bridge"};
  uint32_t update_interval_ms_{1000};
  uint32_t last_publish_ms_{0};
};

}  // namespace redarc_ble
}  // namespace esphome
