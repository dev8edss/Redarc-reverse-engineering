#pragma once

// ESPHome generates src/esphome.h by auto-including every root-level .h file in
// the component directory. The segment headers must stay in the root so ESPHome
// copies them, but tvms_rogue_emulator.h must not include them during that
// auto-include pass or the class is defined twice. Direct .cpp includes have
// include level 1, while the esphome.h auto-include pass is deeper.
#if __INCLUDE_LEVEL__ <= 1
#include "tvms_rogue_emulator_header_00.h"
#include "tvms_rogue_emulator_header_01.h"
#include "tvms_rogue_emulator_header_02.h"
#include "tvms_rogue_emulator_header_03a.h"
#include "tvms_rogue_emulator_header_03b.h"
#include "tvms_rogue_emulator_header_04.h"
#include "tvms_rogue_emulator_header_05.h"
#include "tvms_rogue_emulator_header_06.h"
#include "tvms_rogue_emulator_header_07.h"
#include "tvms_rogue_emulator_header_08.h"
#include "tvms_rogue_emulator_header_09.h"
#include "tvms_rogue_emulator_header_10.h"
#include "tvms_rogue_emulator_header_11.h"
#endif
