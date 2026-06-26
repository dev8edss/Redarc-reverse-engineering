#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

#include <cstdint>

namespace esphome {
namespace redarc_tvms_rogue_emulator {

class TVMSRogueEmulatorComponent;

class TVMSRogueEmulatorLight : public light::LightOutput {
 public:
  void set_parent(TVMSRogueEmulatorComponent *parent) { this->parent_ = parent; }
  void set_output_number(uint8_t output_number) { this->output_number_ = output_number; }
  void setup_state(light::LightState *state) override { this->state_ = state; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  void publish_level(float percent);

 protected:
  TVMSRogueEmulatorComponent *parent_{nullptr};
  light::LightState *state_{nullptr};
  uint8_t output_number_{0};
  bool publishing_feedback_{false};
};

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
