#include "redarc_ble.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace redarc_ble {

static const char *const TAG = "redarc_ble";

void RedarcBLEComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Redarc BLE scaffold");
  ESP_LOGI(TAG, "BLE device name: %s", this->device_name_.c_str());
  ESP_LOGI(TAG, "Sensor demo payload: %s", this->build_demo_sensor_json_().c_str());
}

void RedarcBLEComponent::loop() {
  const uint32_t now = millis();
  if (now - this->last_publish_ms_ >= this->update_interval_ms_) {
    this->last_publish_ms_ = now;
    ESP_LOGD(TAG, "Demo BLE sensor payload: %s", this->build_demo_sensor_json_().c_str());
  }
}

void RedarcBLEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Redarc BLE:");
  ESP_LOGCONFIG(TAG, "  Device name: %s", this->device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Update interval: %ums", this->update_interval_ms_);
}

std::string RedarcBLEComponent::build_demo_sensor_json_() {
  return "{\"soc\":98,\"battery_v\":13.4,\"battery_a\":-4.2,\"solar_w\":95,\"manager_a\":30.6,\"tank_1\":75}";
}

}  // namespace redarc_ble
}  // namespace esphome
