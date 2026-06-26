#pragma once

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
    if (this->configuration_readback_queue_.empty()) return;

    const uint32_t now = millis();
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
    this->source_address_ = v;
    this->register_configuration_readback_listener_();
  }
  void set_identity_interval_ms(uint32_t v) { identity_interval_ms_ = v; }
  void set_status_interval_ms(uint32_t v) { status_interval_ms_ = v; }
  void set_random_update_interval_ms(uint32_t v) { random_update_interval_ms_ = v; }
  void set_randomize_inputs(bool v) { randomize_inputs_ = v; }
  void set_serial_prefix(uint32_t v) { serial_prefix_ = v; }
  void set_serial_suffix(uint16_t v) {
    (void) v;
    serial_suffix_ = 0x0013;
  }
  void set_device_subtype(uint8_t v) { device_subtype_ = v; }
  void add_version_record(uint16_t p, uint8_t a, uint8_t b, uint8_t i) {
    (void) p;
    (void) a;
    (void) b;
    (void) i;
  }
  void set_manufacturing_date(uint8_t d, uint8_t m, uint16_t y) {
    manufacturing_day_ = d;
    manufacturing_month_ = m;
    manufacturing_year_ = y;
  }
  void set_product_name(const std::string &v) { product_name_ = v; }
  void set_unique_identifier(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                             uint8_t e, uint8_t f, uint8_t g) {
    unique_identifier_ = {a, b, c, d, e, f, g};
  }
  void set_unique_identifier_record_index(uint8_t v) {
    unique_identifier_record_index_ = v;
  }

  void set_tank1_sensor(sensor::Sensor *v) { tank1_sensor_ = v; }
  void set_tank2_sensor(sensor::Sensor *v) { tank2_sensor_ = v; }
  void set_input_voltage_sensor(sensor::Sensor *v) { input_voltage_sensor_ = v; }
  void set_input_current_sensor(sensor::Sensor *v) { input_current_sensor_ = v; }
  void set_output_status_text_sensor(text_sensor::TextSensor *v) {
    output_status_text_sensor_ = v;
  }
  void set_level_sensor(uint8_t n, sensor::Sensor *v) {
    if (n >= 1 && n <= 10) level_sensors_[n] = v;
  }
  void set_input_sensor(uint8_t n, binary_sensor::BinarySensor *v) {
    if (n >= 1 && n <= 8) input_sensors_[n] = v;
  }
  void register_light(uint8_t n, TVMSRogueEmulatorLight *v) {
    if (n >= 1 && n <= 10) lights_[n] = v;
  }

  void set_output_from_home_assistant(uint8_t output, float percent);
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);

 protected:
  struct ConfigurationReadbackFrame {
    uint32_t base_can_id;
    std::vector<uint8_t> data;
    uint16_t dgn;
    bool final_frame;
  };

  void send_frame_(uint32_t can_id, const std::vector<uint8_t> &data);
  void send_identity_();
  void send_firmware_versions_();
  void send_manufacturing_date_();
  void send_product_name_();
  void send_unique_identifier_();

  bool load_configuration_object_();
  void patch_configuration_object_();
  void update_configuration_crc_();
  static int8_t base64_value_(char value);
  static void write_u32_le_(std::vector<uint8_t> &target, size_t offset,
                            uint32_t value);

  void send_service_ack_(uint8_t requester, uint8_t opcode);
  void send_direct_ack_(uint8_t requester, uint8_t command);
  void send_empty_block_trailer_(uint8_t requester);
  void send_object_read_block_(uint8_t requester,
                               const std::vector<uint8_t> &request);
  static uint32_t crc32c_(const uint8_t *data, size_t length);

  void randomize_sensor_values_();
  void publish_all_home_assistant_states_();
  void publish_output_status_();
  void set_output_level_(uint8_t output, uint8_t percent, const char *origin);
  void set_master_state_(bool state, const char *origin);
  void set_input_state_(uint8_t input, bool state, const char *origin);
  void send_all_status_(const char *reason);
  void send_channel_status_();
  void send_output_levels_();
  void send_sensor_values_();
  void send_output_capabilities_();
  void send_output_activity_();

  uint32_t read_object_u32_(size_t offset) const {
    if (offset + 4U > this->configuration_object_.size()) return 0;
    return (uint32_t) this->configuration_object_[offset] |
           ((uint32_t) this->configuration_object_[offset + 1U] << 8) |
           ((uint32_t) this->configuration_object_[offset + 2U] << 16) |
           ((uint32_t) this->configuration_object_[offset + 3U] << 24);
  }

  bool read_object_node_(uint32_t offset, uint8_t expected_type,
                         size_t *length = nullptr) const {
    if ((size_t) offset + 4U > this->configuration_object_.size()) return false;
    const uint32_t header = this->read_object_u32_(offset);
    const uint8_t type = (uint8_t) (header >> 24);
    const size_t node_length = (size_t) (header & 0x00FFFFFFUL);
    if (type != expected_type ||
        (size_t) offset + 4U + node_length > this->configuration_object_.size()) {
      return false;
    }
    if (length != nullptr) *length = node_length;
    return true;
  }

  bool object_map_value_(uint32_t map_offset, uint32_t key,
                         uint32_t *value) const {
    size_t length = 0;
    if (!this->read_object_node_(map_offset, 0x03, &length) ||
        (length % 8U) != 0U) {
      return false;
    }
    for (size_t index = 0; index < length; index += 8U) {
      const size_t entry = (size_t) map_offset + 4U + index;
      if (this->read_object_u32_(entry) != key) continue;
      if (value != nullptr) *value = this->read_object_u32_(entry + 4U);
      return true;
    }
    return false;
  }

  bool object_vector_value_(uint32_t vector_offset, size_t index,
                            uint32_t *value) const {
    size_t length = 0;
    if (!this->read_object_node_(vector_offset, 0x02, &length) ||
        (index + 1U) * 4U > length) {
      return false;
    }
    if (value != nullptr) {
      *value = this->read_object_u32_((size_t) vector_offset + 4U + index * 4U);
    }
    return true;
  }

  bool object_string_(uint32_t string_offset, std::string *value) const {
    size_t length = 0;
    if (!this->read_object_node_(string_offset, 0x01, &length)) return false;
    if (value != nullptr) {
      value->assign(
          reinterpret_cast<const char *>(this->configuration_object_.data() +
                                         string_offset + 4U),
          length);
    }
    return true;
  }

  static uint32_t decode_tagged_value_(uint32_t value) {
    return (value & 0x03U) == 0x01U ? (value - 1U) / 4U : value;
  }

  bool rogue_object_root_(uint32_t *rogue_root) const {
    uint32_t root = 0;
    uint32_t devices = 0;
    uint32_t rogue = 0;
    if (!this->object_map_value_(12U, 0x01U, &root) ||
        !this->object_map_value_(root, 0x02U, &devices) ||
        !this->object_map_value_(devices, 0x16U, &rogue)) {
      return false;
    }
    if (rogue_root != nullptr) *rogue_root = rogue;
    return true;
  }

  bool channel_table_(uint32_t *channel_table) const {
    uint32_t rogue_root = 0;
    uint32_t table = 0;
    if (!this->rogue_object_root_(&rogue_root) ||
        !this->object_map_value_(rogue_root, 0x02U, &table)) {
      return false;
    }
    if (channel_table != nullptr) *channel_table = table;
    return true;
  }

  bool channel_record_(uint8_t channel, uint32_t *record) const {
    uint32_t table = 0;
    uint32_t channel_record = 0;
    if (!this->channel_table_(&table) ||
        !this->object_map_value_(table, channel, &channel_record)) {
      return false;
    }
    if (record != nullptr) *record = channel_record;
    return true;
  }

  void sync_identity_from_active_object_() {
    uint32_t rogue_root = 0;
    uint32_t modules = 0;
    size_t module_bytes = 0;
    if (!this->rogue_object_root_(&rogue_root) ||
        !this->object_map_value_(rogue_root, 0x01U, &modules) ||
        !this->read_object_node_(modules, 0x02, &module_bytes)) {
      return;
    }

    uint32_t selected_record = 0;
    uint32_t rogue_fallback = 0;
    for (size_t index = 0; index < module_bytes / 4U; index++) {
      uint32_t record = 0;
      uint32_t identity_offset = 0;
      uint32_t device_type_tagged = 0;
      if (!this->object_vector_value_(modules, index, &record) ||
          !this->object_map_value_(record, 0x01U, &identity_offset)) {
        continue;
      }

      size_t identity_length = 0;
      if (!this->read_object_node_(identity_offset, 0x05, &identity_length) ||
          identity_length < 4U) {
        continue;
      }
      const uint32_t prefix = this->read_object_u32_(identity_offset + 4U);
      if (prefix == this->serial_prefix_) {
        selected_record = record;
        break;
      }

      if (this->object_map_value_(record, 0x03U, &device_type_tagged) &&
          this->decode_tagged_value_(device_type_tagged) == 0x16U) {
        rogue_fallback = record;
      }
    }

    if (selected_record == 0U) selected_record = rogue_fallback;
    if (selected_record == 0U) return;

    uint32_t identity_offset = 0;
    uint32_t suffix_tagged = 0;
    uint32_t name_offset = 0;
    if (!this->object_map_value_(selected_record, 0x01U, &identity_offset) ||
        !this->object_map_value_(selected_record, 0x02U, &suffix_tagged) ||
        !this->object_map_value_(selected_record, 0x04U, &name_offset)) {
      return;
    }

    size_t identity_length = 0;
    std::string object_name;
    if (!this->read_object_node_(identity_offset, 0x05, &identity_length) ||
        identity_length < 4U || !this->object_string_(name_offset, &object_name)) {
      return;
    }

    const uint32_t object_prefix = this->read_object_u32_(identity_offset + 4U);
    const uint32_t decoded_suffix = this->decode_tagged_value_(suffix_tagged);
    const bool changed = object_prefix != this->serial_prefix_ ||
                         decoded_suffix != this->serial_suffix_ ||
                         object_name != this->product_name_;
    this->serial_prefix_ = object_prefix;
    if (decoded_suffix <= 0xFFFFU)
      this->serial_suffix_ = (uint16_t) decoded_suffix;
    this->product_name_ = object_name;

    if (changed) {
      ESP_LOGI("redarc_tvms_rogue_emulator",
               "Active object identity: %010lu-%04u %s",
               (unsigned long) this->serial_prefix_,
               (unsigned) this->serial_suffix_, this->product_name_.c_str());
    }
  }

  void register_configuration_readback_listener_() {
    if (this->configuration_readback_listener_registered_) return;
    this->configuration_readback_listener_registered_ = true;

    redarc_common::RedarcCanDispatcher::instance().add_listener(
        [this](uint32_t can_id, const std::vector<uint8_t> &data) {
          if (data.size() < 2) return;
          const uint32_t id = redarc_common::rvc_id(can_id);
          const uint16_t service = (uint16_t) ((id >> 16) & 0xFFFFU);
          const uint8_t destination = (uint8_t) ((id >> 8) & 0xFFU);
          if (service != 0x0F03U || destination != this->source_address_) return;

          const uint16_t requested_dgn =
              (uint16_t) data[0] | ((uint16_t) data[1] << 8);
          switch (requested_dgn) {
            case 0xF403:
            case 0xF404:
              this->sync_identity_from_active_object_();
              break;
            case 0xFD04:
            case 0xFD06:
            case 0xFD07:
            case 0xFD0A:
            case 0xFD0C:
            case 0xFD0E:
            case 0xFD10:
              this->queue_configuration_readback_(requested_dgn);
              break;
            default:
              break;
          }
        });
  }

  void queue_configuration_frame_(uint16_t dgn, uint32_t base_can_id,
                                  const std::vector<uint8_t> &data,
                                  bool final_frame = false) {
    this->configuration_readback_queue_.push_back(
        {base_can_id, data, dgn, final_frame});
  }

  bool queue_active_labels_(uint16_t dgn, size_t *frame_count) {
    std::array<std::string, 33> labels;
    for (uint8_t channel = 1; channel <= 33; channel++) {
      uint32_t record = 0;
      uint32_t label_offset = 0;
      if (!this->channel_record_(channel, &record) ||
          !this->object_map_value_(record, 0x01U, &label_offset) ||
          !this->object_string_(label_offset, &labels[channel - 1U])) {
        return false;
      }
    }

    for (uint8_t channel = 1; channel <= 33; channel++) {
      const std::string &label = labels[channel - 1U];
      const size_t segments = (label.size() / 6U) + 1U;
      for (size_t segment = 0; segment < segments; segment++) {
        std::vector<uint8_t> frame(8, 0xFF);
        frame[0] = channel;
        frame[1] = (uint8_t) segment;
        for (size_t i = 0; i < 6U; i++) {
          const size_t index = segment * 6U + i;
          if (index < label.size()) frame[i + 2U] = (uint8_t) label[index];
        }
        const bool final = channel == 33U && segment + 1U == segments;
        this->queue_configuration_frame_(dgn, 0x17FD0400UL, frame, final);
        (*frame_count)++;
      }
    }
    return true;
  }

  bool queue_active_fd06_(uint16_t dgn, size_t *frame_count) {
    static const uint8_t channels[] = {0x09, 0x0A, 0x16, 0x17};
    std::array<std::array<uint32_t, 3>, 4> values{};

    for (size_t index = 0; index < 4U; index++) {
      const uint8_t channel = channels[index];
      uint32_t record = 0;
      uint32_t wrapper = 0;
      uint32_t settings = 0;
      if (!this->channel_record_(channel, &record)) return false;
      if (channel <= 0x0AU) {
        if (!this->object_map_value_(record, 0x05U, &wrapper) ||
            !this->object_map_value_(wrapper, 0x07U, &settings)) {
          return false;
        }
      } else {
        if (!this->object_map_value_(record, 0x08U, &wrapper) ||
            !this->object_map_value_(wrapper, 0x01U, &settings)) {
          return false;
        }
      }
      for (uint32_t key = 1; key <= 3; key++) {
        uint32_t tagged = 0;
        if (!this->object_map_value_(settings, key, &tagged)) return false;
        values[index][key - 1U] = this->decode_tagged_value_(tagged);
      }
    }

    for (size_t index = 0; index < 4U; index++) {
      const auto &v = values[index];
      this->queue_configuration_frame_(
          dgn, 0x17FD0600UL,
          {channels[index],
           (uint8_t) v[0],
           (uint8_t) (v[1] & 0xFFU), (uint8_t) ((v[1] >> 8) & 0xFFU),
           (uint8_t) (v[2] & 0xFFU), (uint8_t) ((v[2] >> 8) & 0xFFU),
           0xFF, 0xFF},
          index == 3U);
      (*frame_count)++;
    }
    return true;
  }

  bool queue_active_fd0a_(uint16_t dgn, size_t *frame_count) {
    std::array<std::vector<uint8_t>, 33> frames;
    for (uint8_t channel = 1; channel <= 33; channel++) {
      uint32_t record = 0;
      uint32_t tagged_value = 0;
      if (!this->channel_record_(channel, &record) ||
          !this->object_map_value_(record, 0x03U, &tagged_value)) {
        return false;
      }
      const uint32_t value = this->decode_tagged_value_(tagged_value);

      uint8_t channel_type = 0x00;
      uint16_t subtype = 0x0000;
      if (channel <= 8U) {
        channel_type = 0x00;
      } else if (channel <= 10U) {
        channel_type = 0x0C;
        uint32_t tank = 0;
        uint32_t tank_subtype = 0;
        if (!this->object_map_value_(record, 0x05U, &tank) ||
            !this->object_map_value_(tank, 0x04U, &tank_subtype)) {
          return false;
        }
        subtype = (uint16_t) this->decode_tagged_value_(tank_subtype);
      } else if (channel == 11U) {
        channel_type = 0x08;
        subtype = 0xFFFF;
      } else if (channel <= 21U) {
        channel_type = 0x0A;
        subtype = 0xFFFF;
      } else if (channel <= 23U) {
        channel_type = 0x02;
        subtype = channel == 22U ? 0x0064 : 0x0065;
      } else {
        channel_type = 0x0B;
        subtype = 0xFFFF;
      }

      const uint16_t active = channel <= 23U ? 1U : 0U;
      frames[channel - 1U] = {
          channel, channel_type,
          (uint8_t) (subtype & 0xFFU), (uint8_t) ((subtype >> 8) & 0xFFU),
          (uint8_t) (value & 0xFFU), (uint8_t) ((value >> 8) & 0xFFU),
          (uint8_t) (active & 0xFFU), (uint8_t) ((active >> 8) & 0xFFU)};
    }

    for (size_t index = 0; index < frames.size(); index++) {
      this->queue_configuration_frame_(dgn, 0x17FD0A00UL, frames[index],
                                       index + 1U == frames.size());
      (*frame_count)++;
    }
    return true;
  }

  bool queue_active_fd0e_(uint16_t dgn, size_t *frame_count) {
    std::array<uint8_t, 10> capabilities{};
    for (uint8_t channel = 0x0C; channel <= 0x15; channel++) {
      uint32_t record = 0;
      uint32_t output_settings = 0;
      uint32_t dim_tagged = 0;
      uint32_t switch_tagged = 0;
      if (!this->channel_record_(channel, &record) ||
          !this->object_map_value_(record, 0x06U, &output_settings) ||
          !this->object_map_value_(output_settings, 0x01U, &dim_tagged) ||
          !this->object_map_value_(output_settings, 0x02U, &switch_tagged)) {
        return false;
      }
      uint8_t capability = 0x01;
      if (this->decode_tagged_value_(switch_tagged) != 0U) capability |= 0x02;
      if (this->decode_tagged_value_(dim_tagged) != 0U) capability |= 0x80;
      capabilities[channel - 0x0CU] = capability;
    }

    for (uint8_t channel = 0x0C; channel <= 0x15; channel++) {
      this->queue_configuration_frame_(
          dgn, 0x17FD0E00UL,
          {channel, capabilities[channel - 0x0CU], 0x00,
           0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
          channel == 0x15U);
      (*frame_count)++;
    }
    return true;
  }

  bool queue_active_fd10_(uint16_t dgn, size_t *frame_count) {
    std::array<std::array<uint8_t, 2>, 8> settings{};
    for (uint8_t channel = 1; channel <= 8; channel++) {
      uint32_t record = 0;
      uint32_t input_settings = 0;
      uint32_t first_tagged = 0;
      uint32_t fourth_tagged = 0;
      if (!this->channel_record_(channel, &record) ||
          !this->object_map_value_(record, 0x04U, &input_settings) ||
          !this->object_map_value_(input_settings, 0x01U, &first_tagged) ||
          !this->object_map_value_(input_settings, 0x04U, &fourth_tagged)) {
        return false;
      }
      settings[channel - 1U][0] =
          (uint8_t) this->decode_tagged_value_(first_tagged);
      settings[channel - 1U][1] =
          (uint8_t) this->decode_tagged_value_(fourth_tagged);
    }

    for (uint8_t channel = 1; channel <= 8; channel++) {
      this->queue_configuration_frame_(
          dgn, 0x17FD1000UL,
          {channel, settings[channel - 1U][0], settings[channel - 1U][1],
           0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
          channel == 8U);
      (*frame_count)++;
    }
    return true;
  }

  void queue_configuration_readback_(uint16_t dgn) {
    const uint8_t key = (uint8_t) (dgn & 0xFFU);
    if (this->configuration_dgn_pending_[key]) {
      ESP_LOGD("redarc_tvms_rogue_emulator",
               "Ignored duplicate 0x1%04X request while response is pending",
               dgn);
      return;
    }

    this->configuration_dgn_pending_[key] = true;
    size_t frame_count = 0;
    bool generated = false;

    if (dgn == 0xFD04) {
      generated = this->queue_active_labels_(dgn, &frame_count);
    } else if (dgn == 0xFD06) {
      generated = this->queue_active_fd06_(dgn, &frame_count);
    } else if (dgn == 0xFD07) {
      static const uint8_t frames[][8] = {
          {0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
          {0x16, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
      };
      for (size_t index = 0; index < 2U; index++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0700UL,
            std::vector<uint8_t>(frames[index], frames[index] + 8),
            index == 1U);
        frame_count++;
      }
      generated = true;
    } else if (dgn == 0xFD0A) {
      generated = this->queue_active_fd0a_(dgn, &frame_count);
    } else if (dgn == 0xFD0C) {
      static const uint8_t frames[][8] = {
          {0x09,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
          {0x0A,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
          {0x16,0x61,0x00,0x00,0x00,0x00,0x60,0xEA},
          {0x17,0x61,0x00,0x00,0x00,0x00,0x60,0xEA},
      };
      for (size_t index = 0; index < 4U; index++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0C00UL,
            std::vector<uint8_t>(frames[index], frames[index] + 8),
            index == 3U);
        frame_count++;
      }
      generated = true;
    } else if (dgn == 0xFD0E) {
      generated = this->queue_active_fd0e_(dgn, &frame_count);
    } else if (dgn == 0xFD10) {
      generated = this->queue_active_fd10_(dgn, &frame_count);
    }

    if (!generated || frame_count == 0U) {
      this->configuration_dgn_pending_[key] = false;
      ESP_LOGW("redarc_tvms_rogue_emulator",
               "Could not generate 0x1%04X readback from active object",
               dgn);
      return;
    }

    ESP_LOGD("redarc_tvms_rogue_emulator",
             "Queued %u active-object frames for 0x1%04X configuration readback",
             (unsigned) frame_count, dgn);
  }

  uint8_t source_address_{0x30};
  uint32_t identity_interval_ms_{1000};
  uint32_t status_interval_ms_{1000};
  uint32_t random_update_interval_ms_{5000};
  bool randomize_inputs_{true};
  uint32_t serial_prefix_{0};
  uint16_t serial_suffix_{0x0013};
  uint8_t device_subtype_{0};

  std::vector<VersionRecord> version_records_{{323, 1, 4, 0},
                                               {323, 0, 4, 1}};
  uint8_t manufacturing_day_{1};
  uint8_t manufacturing_month_{1};
  uint16_t manufacturing_year_{2026};
  std::string product_name_{"TVMS Rogue"};
  std::array<uint8_t, 7> unique_identifier_{{0, 0, 0, 0, 0, 0, 1}};
  uint8_t unique_identifier_record_index_{0};
  std::vector<uint8_t> configuration_object_;
  uint8_t selected_object_{0xFF};

  bool configuration_readback_listener_registered_{false};
  std::deque<ConfigurationReadbackFrame> configuration_readback_queue_;
  std::array<bool, 256> configuration_dgn_pending_{{false}};
  uint32_t next_configuration_frame_ms_{0};

  std::array<uint8_t, 11> output_levels_{{0}};
  std::array<bool, 9> input_states_{{false}};
  bool master_state_{false};
  uint8_t tank1_percent_{50};
  uint8_t tank2_percent_{75};
  uint16_t input_voltage_mv_{13500};
  uint16_t input_current_ma_{2500};

  std::array<TVMSRogueEmulatorLight *, 11> lights_{{nullptr}};
  std::array<sensor::Sensor *, 11> level_sensors_{{nullptr}};
  std::array<binary_sensor::BinarySensor *, 9> input_sensors_{{nullptr}};
  sensor::Sensor *tank1_sensor_{nullptr};
  sensor::Sensor *tank2_sensor_{nullptr};
  sensor::Sensor *input_voltage_sensor_{nullptr};
  sensor::Sensor *input_current_sensor_{nullptr};
  text_sensor::TextSensor *output_status_text_sensor_{nullptr};
  std::string last_output_status_;
};

class TVMSRogueActiveEmulatorComponent : public TVMSRogueEmulatorComponent {
 public:
  void setup() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
};

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
