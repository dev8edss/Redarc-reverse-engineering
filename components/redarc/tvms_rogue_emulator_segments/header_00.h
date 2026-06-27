
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/redarc/redarc_common.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "tvms_rogue_emulator_light.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace redarc_tvms_rogue_emulator {

class TVMSRogueEmulatorComponent : public Component {
 public:
  struct VersionRecord {
    uint16_t product_number;
    uint8_t major;
    uint8_t minor;
    uint8_t record_index;
  };

  void setup() override;
  void loop() override {
    const uint32_t now = millis();

    if ((int32_t) (now - this->next_load_disconnect_frame_ms_) >= 0) {
      this->send_load_disconnect_config_();
      this->next_load_disconnect_frame_ms_ = now + 10000U;
    }

    if (this->configuration_readback_queue_.empty()) return;
    if ((int32_t) (now - this->next_configuration_frame_ms_) < 0) return;

    ConfigurationReadbackFrame frame =
        std::move(this->configuration_readback_queue_.front());
    this->configuration_readback_queue_.pop_front();
    this->send_frame_(
        redarc_common::with_sa(frame.base_can_id, this->source_address_),
        frame.data);
    this->next_configuration_frame_ms_ = now + 2U;

    if (frame.final_frame) {
      this->configuration_dgn_pending_[frame.dgn & 0xFFU] = false;
      ESP_LOGD("redarc_tvms_rogue_emulator",
               "Completed active-object 0x1%04X configuration readback",
               frame.dgn);
    }
  }
  void dump_config() override;

  void set_source_address(uint8_t v) {
