#pragma once

#include "tvms_rogue_emulator.h"

namespace esphome {
namespace redarc_tvms_rogue_emulator {

class TVMSRogueActiveEmulatorComponent : public TVMSRogueEmulatorComponent {
 public:
  void setup() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
};

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
