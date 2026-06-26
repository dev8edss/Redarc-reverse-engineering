#include "tvms_rogue_emulator.h"

#include <algorithm>

namespace esphome {
namespace redarc_tvms_rogue_emulator {

static const char *const TAG = "redarc_tvms_rogue_emulator";

namespace {
static constexpr uint8_t DEVICE_TYPE_TVMS_ROGUE = 0x16;

static constexpr uint32_t ID_DEVICE_FIRMWARE_VERSION = 0x17F40000UL;
static constexpr uint32_t ID_DEVICE_MANUFACTURING_DATE = 0x17F40200UL;
static constexpr uint32_t ID_DEVICE_PRODUCT_NAME = 0x17F40300UL;
static constexpr uint32_t ID_DEVICE_SERIAL_IDENTITY = 0x1BF40400UL;
static constexpr uint32_t ID_DEVICE_UNIQUE_IDENTIFIER = 0x1BF40500UL;

static constexpr uint16_t SERVICE_DGN_REQUEST = 0x0F03;
static constexpr uint16_t SERVICE_PROGRAMMING_PREFIX = 0x0E00;
static constexpr uint32_t ID_SERVICE_ACK_BASE = 0x02800000UL;

static constexpr uint16_t REQUEST_DGN_1F400 = 0xF400;
static constexpr uint16_t REQUEST_DGN_1F402 = 0xF402;
static constexpr uint16_t REQUEST_DGN_1F403 = 0xF403;
static constexpr uint16_t REQUEST_DGN_1F404 = 0xF404;
static constexpr uint16_t REQUEST_DGN_1F405 = 0xF405;
}  // namespace

void TVMSRogueEmulatorComponent::setup() {
  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) { this->handle_can_frame(id, data); });

  this->send_identity_();
  this->set_interval("rogue_emulator_identity", this->identity_interval_ms_, [this]() {
    this->send_identity_();
  });
}

void TVMSRogueEmulatorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TVMS Rogue emulator:");
  ESP_LOGCONFIG(TAG, "  Source address: 0x%02X", this->source_address_);
  ESP_LOGCONFIG(TAG, "  Identity interval: %u ms", (unsigned) this->identity_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Serial: %010lu-%04u", (unsigned long) this->serial_prefix_,
                (unsigned) this->serial_suffix_);
  ESP_LOGCONFIG(TAG, "  Product name: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Manufacturing date: %02u/%02u/%04u", (unsigned) this->manufacturing_day_,
                (unsigned) this->manufacturing_month_, (unsigned) this->manufacturing_year_);
  ESP_LOGCONFIG(TAG, "  Firmware/version records: %u", (unsigned) this->version_records_.size());
  ESP_LOGCONFIG(TAG, "  Programming/object services: acknowledged and ignored");
}

void TVMSRogueEmulatorComponent::send_frame_(uint32_t can_id, const std::vector<uint8_t> &data) {
  redarc_common::send_command(can_id, data);
}

void TVMSRogueEmulatorComponent::send_identity_() {
  const std::vector<uint8_t> data = {
      (uint8_t) (this->serial_prefix_ & 0xFFU),
      (uint8_t) ((this->serial_prefix_ >> 8) & 0xFFU),
      (uint8_t) ((this->serial_prefix_ >> 16) & 0xFFU),
      (uint8_t) ((this->serial_prefix_ >> 24) & 0xFFU),
      (uint8_t) (this->serial_suffix_ & 0xFFU),
      (uint8_t) ((this->serial_suffix_ >> 8) & 0xFFU),
      DEVICE_TYPE_TVMS_ROGUE,
      this->device_subtype_,
  };
  this->send_frame_(redarc_common::with_sa(ID_DEVICE_SERIAL_IDENTITY, this->source_address_), data);
}

void TVMSRogueEmulatorComponent::send_firmware_versions_() {
  for (const auto &record : this->version_records_) {
    this->send_frame_(
        redarc_common::with_sa(ID_DEVICE_FIRMWARE_VERSION, this->source_address_),
        {
            (uint8_t) (record.product_number & 0xFFU),
            (uint8_t) ((record.product_number >> 8) & 0xFFU),
            record.major,
            record.minor,
            0x00,
            0x00,
            record.record_index,
            0x00,
        });
  }
}

void TVMSRogueEmulatorComponent::send_manufacturing_date_() {
  this->send_frame_(
      redarc_common::with_sa(ID_DEVICE_MANUFACTURING_DATE, this->source_address_),
      {
          this->manufacturing_day_,
          this->manufacturing_month_,
          (uint8_t) (this->manufacturing_year_ & 0xFFU),
          (uint8_t) ((this->manufacturing_year_ >> 8) & 0xFFU),
          0xFF,
          0xFF,
          0xFF,
          0xFF,
      });
}

void TVMSRogueEmulatorComponent::send_product_name_() {
  const size_t segment_count = std::max<size_t>(1, (this->product_name_.size() + 6U) / 7U);
  const size_t limited_segment_count = std::min<size_t>(segment_count, 256U);

  for (size_t segment = 0; segment < limited_segment_count; segment++) {
    std::vector<uint8_t> data(8, 0xFF);
    data[0] = (uint8_t) segment;
    for (size_t i = 0; i < 7; i++) {
      const size_t index = segment * 7U + i;
      if (index >= this->product_name_.size()) break;
      data[i + 1] = (uint8_t) this->product_name_[index];
    }
    this->send_frame_(redarc_common::with_sa(ID_DEVICE_PRODUCT_NAME, this->source_address_), data);
  }
}

void TVMSRogueEmulatorComponent::send_unique_identifier_() {
  std::vector<uint8_t> data(8, 0x00);
  data[0] = this->unique_identifier_record_index_;
  for (size_t i = 0; i < this->unique_identifier_.size(); i++) data[i + 1] = this->unique_identifier_[i];
  this->send_frame_(redarc_common::with_sa(ID_DEVICE_UNIQUE_IDENTIFIER, this->source_address_), data);
}

bool TVMSRogueEmulatorComponent::is_ignored_programming_opcode_(uint8_t opcode) const {
  switch (opcode) {
    case 0x81:  // block write data
    case 0x83:  // pre-write query
    case 0x85:  // object select
    case 0x86:  // block read request (object service not implemented yet)
    case 0x87:  // object write prepare
    case 0x88:  // block write commit
    case 0x89:  // object close
    case 0x8A:  // object apply
      return true;
    default:
      return false;
  }
}

void TVMSRogueEmulatorComponent::send_programming_ack_(uint8_t requester, uint8_t opcode) {
  const uint32_t response_id = ID_SERVICE_ACK_BASE | ((uint32_t) requester << 8) | this->source_address_;
  this->send_frame_(response_id, {0x00, 0x00, opcode, 0x00, 0x00, 0x00, 0x00, 0x00});
  ESP_LOGI(TAG, "Ignored object/programming service 0x%02X from 0x%02X; sent complete acknowledgement",
           opcode, requester);
}

void TVMSRogueEmulatorComponent::handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  const uint32_t id = redarc_common::rvc_id(can_id);
  const uint16_t service = (uint16_t) ((id >> 16) & 0xFFFFU);
  const uint8_t destination = (uint8_t) ((id >> 8) & 0xFFU);
  const uint8_t requester = (uint8_t) (id & 0xFFU);

  if (destination != this->source_address_) return;

  if (service == SERVICE_DGN_REQUEST) {
    if (data.size() < 2) return;
    const uint16_t requested_dgn = (uint16_t) data[0] | ((uint16_t) data[1] << 8);
    switch (requested_dgn) {
      case REQUEST_DGN_1F400:
        this->send_firmware_versions_();
        break;
      case REQUEST_DGN_1F402:
        this->send_manufacturing_date_();
        break;
      case REQUEST_DGN_1F403:
        this->send_product_name_();
        break;
      case REQUEST_DGN_1F404:
        this->send_identity_();
        break;
      case REQUEST_DGN_1F405:
        this->send_unique_identifier_();
        break;
      default:
        ESP_LOGD(TAG, "Ignoring unsupported DGN request 0x1%04X from 0x%02X", requested_dgn, requester);
        break;
    }
    return;
  }

  if ((service & 0xFF00U) == SERVICE_PROGRAMMING_PREFIX) {
    const uint8_t opcode = (uint8_t) (service & 0xFFU);
    if (this->is_ignored_programming_opcode_(opcode)) this->send_programming_ack_(requester, opcode);
  }
}

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
