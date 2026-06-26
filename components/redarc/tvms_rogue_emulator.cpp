#include "tvms_rogue_emulator.h"
#include "tvms_rogue_object2_baseline.h"

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
static constexpr uint16_t SERVICE_OBJECT_PREFIX = 0x0E00;
static constexpr uint32_t ID_SERVICE_ACK_BASE = 0x02800000UL;
static constexpr uint32_t ID_SERVICE_DATA_BASE = 0x02810000UL;
static constexpr uint32_t ID_SERVICE_TRAILER_BASE = 0x02840000UL;

static constexpr uint16_t REQUEST_DGN_1F400 = 0xF400;
static constexpr uint16_t REQUEST_DGN_1F402 = 0xF402;
static constexpr uint16_t REQUEST_DGN_1F403 = 0xF403;
static constexpr uint16_t REQUEST_DGN_1F404 = 0xF404;
static constexpr uint16_t REQUEST_DGN_1F405 = 0xF405;

static constexpr uint8_t MAIN_CONFIGURATION_OBJECT = 0x02;

// Known fields within the captured Rogue object-2 template. Everything else is
// returned byte-for-byte as captured from the real Rogue.
static constexpr size_t OBJECT_SERIAL_SUFFIX_TAGGED_OFFSET = 0x0088;
static constexpr size_t OBJECT_SERIAL_PREFIX_OFFSET = 0x00A0;
static constexpr size_t OBJECT_PRODUCT_NAME_HEADER_OFFSET = 0x00A8;
static constexpr size_t OBJECT_PRODUCT_NAME_DATA_OFFSET = 0x00AC;
static constexpr size_t OBJECT_PRODUCT_NAME_CAPACITY = 12;
}  // namespace

void TVMSRogueEmulatorComponent::setup() {
  if (!this->load_configuration_object_()) {
    ESP_LOGE(TAG, "Failed to load the captured Rogue configuration object");
    this->mark_failed();
    return;
  }

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
  ESP_LOGCONFIG(TAG, "  Object 2 template: captured real Rogue configuration (%u bytes)",
                (unsigned) this->configuration_object_.size());
  if (this->configuration_object_.size() >= 12) {
    ESP_LOGCONFIG(TAG, "  Object 2 CRC-32C: 0x%08lX",
                  (unsigned long) redarc_common::u32_le(this->configuration_object_, 8));
  }
  ESP_LOGCONFIG(TAG, "  Programming writes: acknowledged and ignored");
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

int8_t TVMSRogueEmulatorComponent::base64_value_(char value) {
  if (value >= 'A' && value <= 'Z') return value - 'A';
  if (value >= 'a' && value <= 'z') return value - 'a' + 26;
  if (value >= '0' && value <= '9') return value - '0' + 52;
  if (value == '+') return 62;
  if (value == '/') return 63;
  return -1;
}

bool TVMSRogueEmulatorComponent::load_configuration_object_() {
  this->configuration_object_.clear();
  this->configuration_object_.reserve(ROGUE_OBJECT2_BASELINE_LENGTH);

  uint32_t accumulator = 0;
  int bits = -8;
  for (const char *cursor = ROGUE_OBJECT2_BASELINE_BASE64; *cursor != '\0'; cursor++) {
    if (*cursor == '=') break;
    const int8_t decoded = this->base64_value_(*cursor);
    if (decoded < 0) continue;
    accumulator = (accumulator << 6) | (uint8_t) decoded;
    bits += 6;
    if (bits >= 0) {
      this->configuration_object_.push_back((uint8_t) ((accumulator >> bits) & 0xFFU));
      bits -= 8;
    }
  }

  if (this->configuration_object_.size() != ROGUE_OBJECT2_BASELINE_LENGTH) {
    ESP_LOGE(TAG, "Decoded Rogue object length %u does not match expected %u",
             (unsigned) this->configuration_object_.size(),
             (unsigned) ROGUE_OBJECT2_BASELINE_LENGTH);
    this->configuration_object_.clear();
    return false;
  }

  const uint32_t declared_length = redarc_common::u32_le(this->configuration_object_, 4);
  if (declared_length != this->configuration_object_.size()) {
    ESP_LOGE(TAG, "Rogue object header length %lu does not match decoded size %u",
             (unsigned long) declared_length, (unsigned) this->configuration_object_.size());
    this->configuration_object_.clear();
    return false;
  }

  this->patch_configuration_object_();
  return true;
}

void TVMSRogueEmulatorComponent::write_u32_le_(std::vector<uint8_t> &target, size_t offset,
                                                uint32_t value) {
  if (offset + 4 > target.size()) return;
  target[offset] = (uint8_t) (value & 0xFFU);
  target[offset + 1] = (uint8_t) ((value >> 8) & 0xFFU);
  target[offset + 2] = (uint8_t) ((value >> 16) & 0xFFU);
  target[offset + 3] = (uint8_t) ((value >> 24) & 0xFFU);
}

void TVMSRogueEmulatorComponent::patch_configuration_object_() {
  if (this->configuration_object_.size() < ROGUE_OBJECT2_BASELINE_LENGTH) return;

  // Keep the captured channel/system structure intact while making the Rogue
  // module entry agree with the identity configured for the emulator.
  this->write_u32_le_(this->configuration_object_, OBJECT_SERIAL_PREFIX_OFFSET,
                      this->serial_prefix_);
  this->write_u32_le_(this->configuration_object_, OBJECT_SERIAL_SUFFIX_TAGGED_OFFSET,
                      ((uint32_t) this->serial_suffix_ * 4U) + 1U);

  if (this->product_name_.size() <= OBJECT_PRODUCT_NAME_CAPACITY) {
    this->write_u32_le_(this->configuration_object_, OBJECT_PRODUCT_NAME_HEADER_OFFSET,
                        0x01000000UL | (uint32_t) this->product_name_.size());
    std::fill(this->configuration_object_.begin() + OBJECT_PRODUCT_NAME_DATA_OFFSET,
              this->configuration_object_.begin() + OBJECT_PRODUCT_NAME_DATA_OFFSET +
                  OBJECT_PRODUCT_NAME_CAPACITY,
              0x00);
    std::copy(this->product_name_.begin(), this->product_name_.end(),
              this->configuration_object_.begin() + OBJECT_PRODUCT_NAME_DATA_OFFSET);
  } else {
    ESP_LOGW(TAG,
             "product_name is longer than the 12-byte slot in the captured object; "
             "DGN 0x1F403 uses the configured name but object 2 keeps 'TVMS Rogue'");
  }

  this->update_configuration_crc_();
}

void TVMSRogueEmulatorComponent::update_configuration_crc_() {
  if (this->configuration_object_.size() < 12) return;
  this->write_u32_le_(this->configuration_object_, 8, 0);
  const uint32_t crc = this->crc32c_(this->configuration_object_.data(),
                                     this->configuration_object_.size());
  this->write_u32_le_(this->configuration_object_, 8, crc);
}

uint32_t TVMSRogueEmulatorComponent::crc32c_(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 1U) ? ((crc >> 1) ^ 0x82F63B78UL) : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

void TVMSRogueEmulatorComponent::send_service_ack_(uint8_t requester, uint8_t opcode) {
  const uint32_t response_id = ID_SERVICE_ACK_BASE | ((uint32_t) requester << 8) | this->source_address_;
  this->send_frame_(response_id, {0x00, 0x00, opcode, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

void TVMSRogueEmulatorComponent::send_empty_block_trailer_(uint8_t requester) {
  const uint32_t response_id = ID_SERVICE_TRAILER_BASE | ((uint32_t) requester << 8) | this->source_address_;
  this->send_frame_(response_id, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
}

void TVMSRogueEmulatorComponent::send_object_read_block_(uint8_t requester,
                                                          const std::vector<uint8_t> &request) {
  if (request.size() < 8) return;

  const uint32_t offset = redarc_common::u32_le(request, 0);
  const uint32_t requested_length = redarc_common::u32_le(request, 4);

  const uint8_t *object_data = nullptr;
  size_t object_length = 0;
  if (this->selected_object_ == MAIN_CONFIGURATION_OBJECT &&
      !this->configuration_object_.empty()) {
    object_data = this->configuration_object_.data();
    object_length = this->configuration_object_.size();
  }

  size_t returned_length = 0;
  if (object_data != nullptr && offset < object_length && requested_length > 0) {
    returned_length = std::min<size_t>((size_t) requested_length,
                                       object_length - (size_t) offset);
  }

  const uint32_t data_id = ID_SERVICE_DATA_BASE | ((uint32_t) requester << 8) | this->source_address_;
  for (size_t block_offset = 0; block_offset < returned_length; block_offset += 8) {
    std::vector<uint8_t> frame(8, 0xFF);
    const size_t chunk = std::min<size_t>(8, returned_length - block_offset);
    for (size_t i = 0; i < chunk; i++) frame[i] = object_data[offset + block_offset + i];
    this->send_frame_(data_id, frame);
  }

  const uint32_t block_crc = this->crc32c_(
      returned_length > 0 ? object_data + offset : nullptr, returned_length);
  const uint32_t trailer_id = ID_SERVICE_TRAILER_BASE | ((uint32_t) requester << 8) | this->source_address_;
  this->send_frame_(
      trailer_id,
      {
          (uint8_t) (returned_length & 0xFFU),
          (uint8_t) ((returned_length >> 8) & 0xFFU),
          (uint8_t) ((returned_length >> 16) & 0xFFU),
          (uint8_t) ((returned_length >> 24) & 0xFFU),
          (uint8_t) (block_crc & 0xFFU),
          (uint8_t) ((block_crc >> 8) & 0xFFU),
          (uint8_t) ((block_crc >> 16) & 0xFFU),
          (uint8_t) ((block_crc >> 24) & 0xFFU),
      });

  ESP_LOGI(TAG, "Object %u read offset=%lu requested=%lu returned=%u",
           (unsigned) this->selected_object_, (unsigned long) offset,
           (unsigned long) requested_length, (unsigned) returned_length);
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

  if ((service & 0xFF00U) != SERVICE_OBJECT_PREFIX) return;

  const uint8_t opcode = (uint8_t) (service & 0xFFU);
  switch (opcode) {
    case 0x81:
      // Write-data frames deliberately disappear into a sink. The later commit is
      // acknowledged, but no bytes are retained or applied.
      ESP_LOGV(TAG, "Ignored object write-data frame from 0x%02X", requester);
      break;

    case 0x83:
      // The real protocol responds to the pre-write query with a 0x0284 trailer,
      // not a generic 0x0280 acknowledgement.
      this->send_empty_block_trailer_(requester);
      ESP_LOGI(TAG, "Ignored pre-write query from 0x%02X", requester);
      break;

    case 0x85:
      this->selected_object_ = data.empty() ? 0xFF : data[0];
      this->send_service_ack_(requester, opcode);
      ESP_LOGI(TAG, "Selected object %u for requester 0x%02X",
               (unsigned) this->selected_object_, requester);
      break;

    case 0x86:
      this->send_object_read_block_(requester, data);
      break;

    case 0x87:
    case 0x88:
    case 0x8A:
      this->send_service_ack_(requester, opcode);
      ESP_LOGI(TAG, "Ignored object/programming service 0x%02X from 0x%02X",
               opcode, requester);
      break;

    case 0x89:
      this->selected_object_ = 0xFF;
      this->send_service_ack_(requester, opcode);
      ESP_LOGI(TAG, "Closed object session for requester 0x%02X", requester);
      break;

    default:
      ESP_LOGD(TAG, "Ignoring unsupported object service 0x%02X from 0x%02X",
               opcode, requester);
      break;
  }
}

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
