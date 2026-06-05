#pragma once
#include <cstdint>
#include <vector>

namespace esphome {
namespace redarc_common {

inline uint16_t u16_le(const std::vector<uint8_t> &data, uint8_t i) {
  if (data.size() <= i + 1) return 0;
  return (uint16_t) data[i] | ((uint16_t) data[i + 1] << 8);
}

inline uint32_t u32_le(const std::vector<uint8_t> &data, uint8_t i) {
  if (data.size() <= i + 3) return 0;
  return ((uint32_t) data[i]) |
         ((uint32_t) data[i + 1] << 8) |
         ((uint32_t) data[i + 2] << 16) |
         ((uint32_t) data[i + 3] << 24);
}

inline uint32_t with_sa(uint32_t id_base, uint8_t source_address) {
  return (id_base & 0x1FFFFF00UL) | source_address;
}

inline float current_32_centered(uint32_t raw) {
  return ((float) raw / 1000.0f) - 1000.0f;
}

inline float current_display_16_centered(uint16_t raw) {
  return ((float) raw / 10.0f) - 1000.0f;
}

}  // namespace redarc_common
}  // namespace esphome
