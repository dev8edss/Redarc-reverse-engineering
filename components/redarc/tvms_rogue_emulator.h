#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/redarc/redarc_common.h"

#include <array>
#include <cstdint>
#include <string>
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
  void dump_config() override;

  void set_source_address(uint8_t source_address) { this->source_address_ = source_address; }
  void set_identity_interval_ms(uint32_t interval_ms) { this->identity_interval_ms_ = interval_ms; }
  void set_serial_prefix(uint32_t serial_prefix) { this->serial_prefix_ = serial_prefix; }
  void set_serial_suffix(uint16_t serial_suffix) { this->serial_suffix_ = serial_suffix; }
  void set_device_subtype(uint8_t subtype) { this->device_subtype_ = subtype; }

  void add_version_record(uint16_t product_number, uint8_t major, uint8_t minor, uint8_t record_index) {
    this->version_records_.push_back({product_number, major, minor, record_index});
  }

  void set_manufacturing_date(uint8_t day, uint8_t month, uint16_t year) {
    this->manufacturing_day_ = day;
    this->manufacturing_month_ = month;
    this->manufacturing_year_ = year;
  }

  void set_product_name(const std::string &product_name) { this->product_name_ = product_name; }

  void set_unique_identifier(uint8_t byte_1, uint8_t byte_2, uint8_t byte_3, uint8_t byte_4,
                             uint8_t byte_5, uint8_t byte_6, uint8_t byte_7) {
    this->unique_identifier_ = {byte_1, byte_2, byte_3, byte_4, byte_5, byte_6, byte_7};
  }

  void set_unique_identifier_record_index(uint8_t record_index) {
    this->unique_identifier_record_index_ = record_index;
  }

  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);

 protected:
  void send_frame_(uint32_t can_id, const std::vector<uint8_t> &data);
  void send_identity_();
  void send_firmware_versions_();
  void send_manufacturing_date_();
  void send_product_name_();
  void send_unique_identifier_();
  void send_programming_ack_(uint8_t requester, uint8_t opcode);
  bool is_ignored_programming_opcode_(uint8_t opcode) const;

  uint8_t source_address_{0x30};
  uint32_t identity_interval_ms_{1000};
  uint32_t serial_prefix_{0};
  uint16_t serial_suffix_{1};
  uint8_t device_subtype_{0};

  std::vector<VersionRecord> version_records_;
  uint8_t manufacturing_day_{1};
  uint8_t manufacturing_month_{1};
  uint16_t manufacturing_year_{2026};
  std::string product_name_{"TVMS Rogue"};
  std::array<uint8_t, 7> unique_identifier_{{0, 0, 0, 0, 0, 0, 1}};
  uint8_t unique_identifier_record_index_{0};
};

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
