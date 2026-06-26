#include "tvms_rogue_emulator.h"
#include "tvms_rogue_object2_baseline.h"

#include "esphome/core/preferences.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

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
static constexpr uint32_t WRITE_WINDOW_SIZE = 0x00000400UL;
static constexpr size_t MAX_CONFIGURATION_OBJECT_SIZE = 8192;
static constexpr size_t RECEIVED_BITMAP_SIZE =
    (MAX_CONFIGURATION_OBJECT_SIZE + 7U) / 8U;

static constexpr uint32_t PERSISTED_CONFIGURATION_MAGIC = 0x52474346UL;
static constexpr uint16_t PERSISTED_CONFIGURATION_VERSION = 1;
static constexpr uint32_t PREFERENCE_KEY_BASE = 0x524F4702UL;

static constexpr uint8_t PROGRAMMING_STATUS_OK = 0x00;
static constexpr uint8_t PROGRAMMING_STATUS_BUSY = 0x01;
static constexpr uint8_t PROGRAMMING_STATUS_ERROR = 0x03;

static constexpr size_t OBJECT_SERIAL_SUFFIX_TAGGED_OFFSET = 0x0088;
static constexpr size_t OBJECT_SERIAL_PREFIX_OFFSET = 0x00A0;
static constexpr size_t OBJECT_PRODUCT_NAME_HEADER_OFFSET = 0x00A8;
static constexpr size_t OBJECT_PRODUCT_NAME_DATA_OFFSET = 0x00AC;
static constexpr size_t OBJECT_PRODUCT_NAME_CAPACITY = 12;

struct PersistedConfigurationBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t length;
  uint32_t data_crc32c;
  std::array<uint8_t, MAX_CONFIGURATION_OBJECT_SIZE> data;
};

struct ComponentWriteState {
  const TVMSRogueEmulatorComponent *owner{nullptr};
  bool preference_initialized{false};
  bool loaded_from_flash{false};
  bool write_active{false};
  bool write_closed{false};
  bool block_overflow{false};
  uint8_t write_object{0xFF};
  ESPPreferenceObject preference;
  std::vector<uint8_t> block_buffer;
  std::array<uint8_t, MAX_CONFIGURATION_OBJECT_SIZE> staging{};
  std::array<uint8_t, RECEIVED_BITMAP_SIZE> received{};
};

std::vector<std::unique_ptr<ComponentWriteState>> &write_states() {
  static std::vector<std::unique_ptr<ComponentWriteState>> states;
  return states;
}

ComponentWriteState &write_state_for(const TVMSRogueEmulatorComponent *owner) {
  for (auto &state : write_states()) {
    if (state->owner == owner) return *state;
  }

  auto state = std::make_unique<ComponentWriteState>();
  state->owner = owner;
  state->staging.fill(0xFF);
  auto *result = state.get();
  write_states().push_back(std::move(state));
  return *result;
}

uint32_t read_u32_le_raw(const uint8_t *data) {
  return (uint32_t) data[0] |
         ((uint32_t) data[1] << 8) |
         ((uint32_t) data[2] << 16) |
         ((uint32_t) data[3] << 24);
}

uint32_t crc32c_bytes(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 1U)
                ? ((crc >> 1) ^ 0x82F63B78UL)
                : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

uint32_t configuration_crc32c(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; i++) {
    const uint8_t value = (i >= 8U && i < 12U) ? 0x00 : data[i];
    crc ^= value;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 1U)
                ? ((crc >> 1) ^ 0x82F63B78UL)
                : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

bool validate_configuration_object(const uint8_t *data, size_t available,
                                   size_t *declared_length,
                                   uint32_t *stored_crc,
                                   uint32_t *calculated_crc) {
  if (data == nullptr || available < 12U) return false;

  const uint32_t length = read_u32_le_raw(data + 4U);
  if (length < 12U || length > available ||
      length > MAX_CONFIGURATION_OBJECT_SIZE) {
    return false;
  }

  const uint32_t expected = read_u32_le_raw(data + 8U);
  const uint32_t actual = configuration_crc32c(data, length);

  if (declared_length != nullptr) *declared_length = length;
  if (stored_crc != nullptr) *stored_crc = expected;
  if (calculated_crc != nullptr) *calculated_crc = actual;
  return expected == actual;
}

void initialize_preference(ComponentWriteState &state, uint8_t source_address) {
  if (state.preference_initialized || global_preferences == nullptr) return;
  const uint32_t key = PREFERENCE_KEY_BASE ^ (uint32_t) source_address;
  state.preference =
      global_preferences->make_preference<PersistedConfigurationBlob>(key);
  state.preference_initialized = true;
}

bool load_persisted_configuration(ComponentWriteState &state,
                                  std::vector<uint8_t> &target) {
  if (!state.preference_initialized) return false;

  auto blob = std::make_unique<PersistedConfigurationBlob>();
  if (!state.preference.load(blob.get())) return false;
  if (blob->magic != PERSISTED_CONFIGURATION_MAGIC ||
      blob->version != PERSISTED_CONFIGURATION_VERSION ||
      blob->length < 12U ||
      blob->length > MAX_CONFIGURATION_OBJECT_SIZE) {
    ESP_LOGW(TAG, "Stored Rogue configuration metadata is invalid; using factory object");
    return false;
  }

  const uint32_t stored_data_crc =
      crc32c_bytes(blob->data.data(), blob->length);
  if (stored_data_crc != blob->data_crc32c) {
    ESP_LOGW(TAG,
             "Stored Rogue configuration flash CRC mismatch: expected=0x%08lX actual=0x%08lX",
             (unsigned long) blob->data_crc32c,
             (unsigned long) stored_data_crc);
    return false;
  }

  size_t declared_length = 0;
  uint32_t object_crc = 0;
  uint32_t calculated_crc = 0;
  if (!validate_configuration_object(blob->data.data(), blob->length,
                                     &declared_length, &object_crc,
                                     &calculated_crc) ||
      declared_length != blob->length) {
    ESP_LOGW(TAG,
             "Stored Rogue configuration object is invalid: length=%lu object_crc=0x%08lX calculated=0x%08lX",
             (unsigned long) blob->length,
             (unsigned long) object_crc,
             (unsigned long) calculated_crc);
    return false;
  }

  target.assign(blob->data.begin(), blob->data.begin() + blob->length);
  state.loaded_from_flash = true;
  ESP_LOGI(TAG,
           "Loaded persisted Rogue configuration: %lu bytes CRC-32C=0x%08lX",
           (unsigned long) blob->length,
           (unsigned long) object_crc);
  return true;
}

bool persist_configuration(ComponentWriteState &state,
                           const uint8_t *data, size_t length) {
  if (!state.preference_initialized || global_preferences == nullptr ||
      data == nullptr || length < 12U ||
      length > MAX_CONFIGURATION_OBJECT_SIZE) {
    return false;
  }

  auto blob = std::make_unique<PersistedConfigurationBlob>();
  blob->magic = PERSISTED_CONFIGURATION_MAGIC;
  blob->version = PERSISTED_CONFIGURATION_VERSION;
  blob->reserved = 0;
  blob->length = (uint32_t) length;
  blob->data.fill(0xFF);
  std::copy_n(data, length, blob->data.begin());
  blob->data_crc32c = crc32c_bytes(blob->data.data(), length);

  if (!state.preference.save(blob.get())) return false;
  return global_preferences->sync();
}

void reset_write_transaction(ComponentWriteState &state, uint8_t object) {
  state.write_active = object == MAIN_CONFIGURATION_OBJECT;
  state.write_closed = false;
  state.block_overflow = false;
  state.write_object = object;
  state.block_buffer.clear();
  state.block_buffer.reserve(WRITE_WINDOW_SIZE);
  state.staging.fill(0xFF);
  state.received.fill(0x00);
}

void finish_write_transaction(ComponentWriteState &state) {
  state.write_active = false;
  state.write_closed = false;
  state.block_overflow = false;
  state.write_object = 0xFF;
  state.block_buffer.clear();
  state.received.fill(0x00);
}

void mark_received(ComponentWriteState &state, size_t offset, size_t length) {
  for (size_t index = offset; index < offset + length; index++) {
    state.received[index >> 3U] |= (uint8_t) (1U << (index & 7U));
  }
}

bool range_received(const ComponentWriteState &state, size_t length) {
  for (size_t index = 0; index < length; index++) {
    if ((state.received[index >> 3U] &
         (uint8_t) (1U << (index & 7U))) == 0) {
      return false;
    }
  }
  return true;
}
}  // namespace

void TVMSRogueEmulatorComponent::setup() {
  if (!this->load_configuration_object_()) {
    ESP_LOGE(TAG, "Failed to load the Rogue configuration object");
    this->mark_failed();
    return;
  }

  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) {
        this->handle_can_frame(id, data);
      });

  this->send_identity_();
  this->set_interval("rogue_emulator_identity", this->identity_interval_ms_,
                     [this]() { this->send_identity_(); });
}

void TVMSRogueEmulatorComponent::dump_config() {
  const auto &state = write_state_for(this);
  ESP_LOGCONFIG(TAG, "TVMS Rogue emulator:");
  ESP_LOGCONFIG(TAG, "  Source address: 0x%02X", this->source_address_);
  ESP_LOGCONFIG(TAG, "  Identity interval: %u ms",
                (unsigned) this->identity_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Serial: %010lu-%04u",
                (unsigned long) this->serial_prefix_,
                (unsigned) this->serial_suffix_);
  ESP_LOGCONFIG(TAG, "  Product name: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Manufacturing date: %02u/%02u/%04u",
                (unsigned) this->manufacturing_day_,
                (unsigned) this->manufacturing_month_,
                (unsigned) this->manufacturing_year_);
  ESP_LOGCONFIG(TAG, "  Firmware/version records: %u",
                (unsigned) this->version_records_.size());
  ESP_LOGCONFIG(TAG, "  Object 2 source: %s",
                state.loaded_from_flash ? "persisted flash configuration"
                                        : "factory captured template");
  ESP_LOGCONFIG(TAG, "  Object 2 length: %u bytes",
                (unsigned) this->configuration_object_.size());
  if (this->configuration_object_.size() >= 12) {
    ESP_LOGCONFIG(TAG, "  Object 2 CRC-32C: 0x%08lX",
                  (unsigned long) redarc_common::u32_le(
                      this->configuration_object_, 8));
  }
  ESP_LOGCONFIG(TAG, "  Programming writes: validated, persisted and read back");
  ESP_LOGCONFIG(TAG, "  Maximum persisted object: %u bytes",
                (unsigned) MAX_CONFIGURATION_OBJECT_SIZE);
  ESP_LOGCONFIG(TAG, "  Write transfer window: %lu bytes",
                (unsigned long) WRITE_WINDOW_SIZE);
}

void TVMSRogueEmulatorComponent::send_frame_(
    uint32_t can_id, const std::vector<uint8_t> &data) {
  redarc_common::send_command(can_id, data);
}

void TVMSRogueEmulatorComponent::send_identity_() {
  this->send_frame_(
      redarc_common::with_sa(ID_DEVICE_SERIAL_IDENTITY,
                             this->source_address_),
      {
          (uint8_t) (this->serial_prefix_ & 0xFFU),
          (uint8_t) ((this->serial_prefix_ >> 8) & 0xFFU),
          (uint8_t) ((this->serial_prefix_ >> 16) & 0xFFU),
          (uint8_t) ((this->serial_prefix_ >> 24) & 0xFFU),
          (uint8_t) (this->serial_suffix_ & 0xFFU),
          (uint8_t) ((this->serial_suffix_ >> 8) & 0xFFU),
          DEVICE_TYPE_TVMS_ROGUE,
          this->device_subtype_,
      });
}

void TVMSRogueEmulatorComponent::send_firmware_versions_() {
  for (const auto &record : this->version_records_) {
    this->send_frame_(
        redarc_common::with_sa(ID_DEVICE_FIRMWARE_VERSION,
                               this->source_address_),
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
      redarc_common::with_sa(ID_DEVICE_MANUFACTURING_DATE,
                             this->source_address_),
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
  const size_t segment_count =
      std::max<size_t>(1, (this->product_name_.size() + 6U) / 7U);
  const size_t limited_segment_count =
      std::min<size_t>(segment_count, 256U);

  for (size_t segment = 0; segment < limited_segment_count; segment++) {
    std::vector<uint8_t> data(8, 0xFF);
    data[0] = (uint8_t) segment;
    for (size_t i = 0; i < 7; i++) {
      const size_t index = segment * 7U + i;
      if (index >= this->product_name_.size()) break;
      data[i + 1] = (uint8_t) this->product_name_[index];
    }
    this->send_frame_(
        redarc_common::with_sa(ID_DEVICE_PRODUCT_NAME,
                               this->source_address_),
        data);
  }
}

void TVMSRogueEmulatorComponent::send_unique_identifier_() {
  std::vector<uint8_t> data(8, 0x00);
  data[0] = this->unique_identifier_record_index_;
  for (size_t i = 0; i < this->unique_identifier_.size(); i++)
    data[i + 1] = this->unique_identifier_[i];
  this->send_frame_(
      redarc_common::with_sa(ID_DEVICE_UNIQUE_IDENTIFIER,
                             this->source_address_),
      data);
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
  for (const char *cursor = ROGUE_OBJECT2_BASELINE_BASE64;
       *cursor != '\0'; cursor++) {
    if (*cursor == '=') break;
    const int8_t decoded = this->base64_value_(*cursor);
    if (decoded < 0) continue;
    accumulator = (accumulator << 6) | (uint8_t) decoded;
    bits += 6;
    if (bits >= 0) {
      this->configuration_object_.push_back(
          (uint8_t) ((accumulator >> bits) & 0xFFU));
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

  const uint32_t declared_length =
      redarc_common::u32_le(this->configuration_object_, 4);
  if (declared_length != this->configuration_object_.size()) {
    ESP_LOGE(TAG, "Rogue object header length %lu does not match decoded size %u",
             (unsigned long) declared_length,
             (unsigned) this->configuration_object_.size());
    this->configuration_object_.clear();
    return false;
  }

  this->patch_configuration_object_();

  auto &state = write_state_for(this);
  initialize_preference(state, this->source_address_);
  load_persisted_configuration(state, this->configuration_object_);
  return true;
}

void TVMSRogueEmulatorComponent::write_u32_le_(
    std::vector<uint8_t> &target, size_t offset, uint32_t value) {
  if (offset + 4 > target.size()) return;
  target[offset] = (uint8_t) (value & 0xFFU);
  target[offset + 1] = (uint8_t) ((value >> 8) & 0xFFU);
  target[offset + 2] = (uint8_t) ((value >> 16) & 0xFFU);
  target[offset + 3] = (uint8_t) ((value >> 24) & 0xFFU);
}

void TVMSRogueEmulatorComponent::patch_configuration_object_() {
  if (this->configuration_object_.size() < ROGUE_OBJECT2_BASELINE_LENGTH)
    return;

  this->write_u32_le_(this->configuration_object_,
                      OBJECT_SERIAL_PREFIX_OFFSET,
                      this->serial_prefix_);
  this->write_u32_le_(this->configuration_object_,
                      OBJECT_SERIAL_SUFFIX_TAGGED_OFFSET,
                      ((uint32_t) this->serial_suffix_ * 4U) + 1U);

  if (this->product_name_.size() <= OBJECT_PRODUCT_NAME_CAPACITY) {
    this->write_u32_le_(this->configuration_object_,
                        OBJECT_PRODUCT_NAME_HEADER_OFFSET,
                        0x01000000UL |
                            (uint32_t) this->product_name_.size());
    std::fill(this->configuration_object_.begin() +
                  OBJECT_PRODUCT_NAME_DATA_OFFSET,
              this->configuration_object_.begin() +
                  OBJECT_PRODUCT_NAME_DATA_OFFSET +
                  OBJECT_PRODUCT_NAME_CAPACITY,
              0x00);
    std::copy(this->product_name_.begin(), this->product_name_.end(),
              this->configuration_object_.begin() +
                  OBJECT_PRODUCT_NAME_DATA_OFFSET);
  } else {
    ESP_LOGW(TAG,
             "product_name is longer than the captured 12-byte slot; object 2 keeps TVMS Rogue");
  }

  this->update_configuration_crc_();
}

void TVMSRogueEmulatorComponent::update_configuration_crc_() {
  if (this->configuration_object_.size() < 12) return;
  this->write_u32_le_(this->configuration_object_, 8, 0);
  const uint32_t crc = this->crc32c_(
      this->configuration_object_.data(),
      this->configuration_object_.size());
  this->write_u32_le_(this->configuration_object_, 8, crc);
}

uint32_t TVMSRogueEmulatorComponent::crc32c_(const uint8_t *data,
                                             size_t length) {
  return crc32c_bytes(data, length);
}

void TVMSRogueEmulatorComponent::send_service_ack_(uint8_t requester,
                                                    uint8_t opcode) {
  const uint32_t response_id =
      ID_SERVICE_ACK_BASE | ((uint32_t) requester << 8) |
      this->source_address_;
  this->send_frame_(response_id,
                    {PROGRAMMING_STATUS_OK, 0x00, opcode, 0xFF,
                     0xFF, 0xFF, 0xFF, 0xFF});
}

void TVMSRogueEmulatorComponent::send_empty_block_trailer_(
    uint8_t requester) {
  const uint32_t response_id =
      ID_SERVICE_TRAILER_BASE | ((uint32_t) requester << 8) |
      this->source_address_;
  this->send_frame_(response_id,
                    {0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00});
}

void TVMSRogueEmulatorComponent::send_object_read_block_(
    uint8_t requester, const std::vector<uint8_t> &request) {
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
  if (object_data != nullptr && offset < object_length &&
      requested_length > 0) {
    returned_length = std::min<size_t>(
        (size_t) requested_length,
        object_length - (size_t) offset);
  }

  const uint32_t data_id =
      ID_SERVICE_DATA_BASE | ((uint32_t) requester << 8) |
      this->source_address_;
  for (size_t block_offset = 0;
       block_offset < returned_length; block_offset += 8) {
    std::vector<uint8_t> frame(8, 0xFF);
    const size_t chunk = std::min<size_t>(
        8, returned_length - block_offset);
    for (size_t i = 0; i < chunk; i++)
      frame[i] = object_data[offset + block_offset + i];
    this->send_frame_(data_id, frame);
  }

  const uint32_t block_crc = this->crc32c_(
      returned_length > 0 ? object_data + offset : nullptr,
      returned_length);
  const uint32_t trailer_id =
      ID_SERVICE_TRAILER_BASE | ((uint32_t) requester << 8) |
      this->source_address_;
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

  ESP_LOGI(TAG,
           "Object %u read offset=%lu requested=%lu returned=%u",
           (unsigned) this->selected_object_,
           (unsigned long) offset,
           (unsigned long) requested_length,
           (unsigned) returned_length);
}

void TVMSRogueEmulatorComponent::handle_can_frame(
    uint32_t can_id, const std::vector<uint8_t> &data) {
  const uint32_t id = redarc_common::rvc_id(can_id);
  const uint16_t service =
      (uint16_t) ((id >> 16) & 0xFFFFU);
  const uint8_t destination =
      (uint8_t) ((id >> 8) & 0xFFU);
  const uint8_t requester = (uint8_t) (id & 0xFFU);

  if (destination != this->source_address_) return;

  if (service == SERVICE_DGN_REQUEST) {
    if (data.size() < 2) return;
    const uint16_t requested_dgn =
        (uint16_t) data[0] | ((uint16_t) data[1] << 8);
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
        ESP_LOGD(TAG,
                 "Ignoring unsupported DGN request 0x1%04X from 0x%02X",
                 requested_dgn, requester);
        break;
    }
    return;
  }

  if ((service & 0xFF00U) != SERVICE_OBJECT_PREFIX) return;

  auto &write_state = write_state_for(this);
  const uint32_t response_id =
      ID_SERVICE_ACK_BASE | ((uint32_t) requester << 8) |
      this->source_address_;
  const auto send_programming_status =
      [this, response_id](uint8_t status, uint8_t opcode) {
        this->send_frame_(response_id,
                          {status, 0x00, opcode, 0xFF,
                           0xFF, 0xFF, 0xFF, 0xFF});
      };

  const uint8_t opcode = (uint8_t) (service & 0xFFU);
  switch (opcode) {
    case 0x81:
      if (!write_state.write_active) {
        ESP_LOGW(TAG,
                 "Ignored object write data without an active write session from 0x%02X",
                 requester);
        break;
      }
      if (write_state.block_buffer.size() + data.size() > WRITE_WINDOW_SIZE) {
        write_state.block_overflow = true;
        ESP_LOGW(TAG,
                 "Write block exceeded %lu-byte transfer window from 0x%02X",
                 (unsigned long) WRITE_WINDOW_SIZE, requester);
        break;
      }
      write_state.block_buffer.insert(write_state.block_buffer.end(),
                                      data.begin(), data.end());
      ESP_LOGV(TAG, "Buffered %u write bytes from 0x%02X; block now %u bytes",
               (unsigned) data.size(), requester,
               (unsigned) write_state.block_buffer.size());
      break;

    case 0x83: {
      const uint32_t data_id =
          ID_SERVICE_DATA_BASE | ((uint32_t) requester << 8) |
          this->source_address_;
      const uint32_t trailer_id =
          ID_SERVICE_TRAILER_BASE | ((uint32_t) requester << 8) |
          this->source_address_;
      this->send_frame_(data_id,
                        {0x00, 0x04, 0x00, 0x00,
                         0xFF, 0xFF, 0xFF, 0xFF});
      this->send_frame_(trailer_id,
                        {0x04, 0x00, 0x00, 0x00,
                         0xDD, 0xEF, 0xB9, 0xD6});
      ESP_LOGI(TAG,
               "Reported pre-write capability to requester 0x%02X: window=1024 bytes",
               requester);
      break;
    }

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
      reset_write_transaction(write_state, this->selected_object_);
      if (write_state.write_active) {
        send_programming_status(PROGRAMMING_STATUS_OK, opcode);
        ESP_LOGI(TAG,
                 "Started transactional write for object %u requester 0x%02X",
                 (unsigned) write_state.write_object, requester);
      } else {
        send_programming_status(PROGRAMMING_STATUS_ERROR, opcode);
        ESP_LOGW(TAG,
                 "Rejected write for unsupported object %u requester 0x%02X",
                 (unsigned) this->selected_object_, requester);
      }
      break;

    case 0x88: {
      send_programming_status(PROGRAMMING_STATUS_BUSY, opcode);

      bool valid = write_state.write_active && data.size() >= 8U &&
                   !write_state.block_overflow &&
                   !write_state.block_buffer.empty();
      const uint32_t offset = data.size() >= 4U
                                  ? redarc_common::u32_le(data, 0)
                                  : 0;
      const uint32_t expected_crc = data.size() >= 8U
                                        ? redarc_common::u32_le(data, 4)
                                        : 0;
      const uint32_t actual_crc = crc32c_bytes(
          write_state.block_buffer.data(),
          write_state.block_buffer.size());

      if (write_state.block_buffer.size() > WRITE_WINDOW_SIZE ||
          offset > MAX_CONFIGURATION_OBJECT_SIZE ||
          write_state.block_buffer.size() >
              MAX_CONFIGURATION_OBJECT_SIZE -
                  std::min<size_t>(offset, MAX_CONFIGURATION_OBJECT_SIZE)) {
        valid = false;
      }
      if (valid && actual_crc != expected_crc) valid = false;

      if (valid) {
        std::copy(write_state.block_buffer.begin(),
                  write_state.block_buffer.end(),
                  write_state.staging.begin() + offset);
        mark_received(write_state, offset,
                      write_state.block_buffer.size());
        send_programming_status(PROGRAMMING_STATUS_OK, opcode);
        ESP_LOGI(TAG,
                 "Accepted write block offset=0x%04lX length=%u CRC-32C=0x%08lX",
                 (unsigned long) offset,
                 (unsigned) write_state.block_buffer.size(),
                 (unsigned long) actual_crc);
      } else {
        send_programming_status(PROGRAMMING_STATUS_ERROR, opcode);
        ESP_LOGW(TAG,
                 "Rejected write block offset=0x%04lX length=%u expected_crc=0x%08lX actual_crc=0x%08lX",
                 (unsigned long) offset,
                 (unsigned) write_state.block_buffer.size(),
                 (unsigned long) expected_crc,
                 (unsigned long) actual_crc);
      }

      write_state.block_buffer.clear();
      write_state.block_overflow = false;
      break;
    }

    case 0x89: {
      if (!write_state.write_active) {
        this->selected_object_ = 0xFF;
        send_programming_status(PROGRAMMING_STATUS_OK, opcode);
        ESP_LOGI(TAG, "Closed object read session for requester 0x%02X",
                 requester);
        break;
      }

      const bool valid =
          write_state.write_object == MAIN_CONFIGURATION_OBJECT &&
          write_state.block_buffer.empty() &&
          !write_state.block_overflow;
      write_state.write_active = false;
      write_state.write_closed = valid;
      this->selected_object_ = 0xFF;
      send_programming_status(valid ? PROGRAMMING_STATUS_OK
                                    : PROGRAMMING_STATUS_ERROR,
                              opcode);
      ESP_LOGI(TAG, "%s object write session for requester 0x%02X",
               valid ? "Closed" : "Rejected close of", requester);
      break;
    }

    case 0x8A: {
      bool valid = write_state.write_closed &&
                   write_state.write_object == MAIN_CONFIGURATION_OBJECT;
      size_t declared_length = 0;
      uint32_t stored_crc = 0;
      uint32_t calculated_crc = 0;

      if (valid) {
        valid = validate_configuration_object(
            write_state.staging.data(),
            MAX_CONFIGURATION_OBJECT_SIZE,
            &declared_length, &stored_crc, &calculated_crc);
      }
      if (valid) valid = range_received(write_state, declared_length);

      bool persisted = false;
      if (valid) {
        persisted = persist_configuration(write_state,
                                          write_state.staging.data(),
                                          declared_length);
        valid = persisted;
      }

      if (valid) {
        this->configuration_object_.assign(
            write_state.staging.begin(),
            write_state.staging.begin() + declared_length);
        write_state.loaded_from_flash = true;
        send_programming_status(PROGRAMMING_STATUS_OK, opcode);
        ESP_LOGI(TAG,
                 "Committed and persisted Rogue object 2: %u bytes CRC-32C=0x%08lX",
                 (unsigned) declared_length,
                 (unsigned long) stored_crc);
      } else {
        send_programming_status(PROGRAMMING_STATUS_ERROR, opcode);
        ESP_LOGE(TAG,
                 "Rejected final Rogue configuration commit: length=%u stored_crc=0x%08lX calculated_crc=0x%08lX complete=%s persisted=%s",
                 (unsigned) declared_length,
                 (unsigned long) stored_crc,
                 (unsigned long) calculated_crc,
                 declared_length > 0U &&
                         range_received(write_state, declared_length)
                     ? "yes"
                     : "no",
                 persisted ? "yes" : "no");
      }

      finish_write_transaction(write_state);
      break;
    }

    default:
      ESP_LOGD(TAG,
               "Ignoring unsupported object service 0x%02X from 0x%02X",
               opcode, requester);
      break;
  }
}

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
