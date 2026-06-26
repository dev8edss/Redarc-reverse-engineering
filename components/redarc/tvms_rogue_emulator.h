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
      ESP_LOGI("redarc_tvms_rogue_emulator",
               "Completed captured 0x1%04X configuration readback",
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
    // Keep the captured Rogue suffix because object 2 contains the tagged value
    // 0x4D at offset 0x88 and the DGN identity must agree with that object.
    serial_suffix_ = 0x0013;
  }
  void set_device_subtype(uint8_t v) { device_subtype_ = v; }
  void add_version_record(uint16_t p, uint8_t a, uint8_t b, uint8_t i) {
    (void) p;
    (void) a;
    (void) b;
    (void) i;
    // The captured object belongs to Rogue firmware records 1.4 and 0.4.
    // Configured alternatives are intentionally ignored for parser compatibility.
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

  void queue_configuration_readback_(uint16_t dgn) {
    const uint8_t key = (uint8_t) (dgn & 0xFFU);
    if (this->configuration_dgn_pending_[key]) {
      ESP_LOGI("redarc_tvms_rogue_emulator",
               "Ignored duplicate 0x1%04X request while response is pending",
               dgn);
      return;
    }

    this->configuration_dgn_pending_[key] = true;
    size_t frame_count = 0;

    if (dgn == 0xFD04) {
      static const char *const labels[] = {
          "Left", "Strip", "Dome", "Digital Input 4", "Digital Input 5",
          "Digital Input 6", "Digital Input 7", "Digital Input 8", "Rear",
          "Front", "Master", "Left", "Right", "Rear", "Kitchen",
          "Handle", "LED", "Dome Light", "Lights", "Amber Lights",
          "Amber Kitchen", "Rogue Input Voltage", "Input Current",
          "Remote Input  1", "Remote Input  2", "Remote Input  3",
          "Remote Input  4", "Remote Input  5", "Remote Input  6",
          "Remote Input  7", "Remote Input  8", "Remote Input  9",
          "Remote Input  10"};

      for (uint8_t channel = 1; channel <= 33; channel++) {
        const char *label = labels[channel - 1];
        const size_t length = std::strlen(label);
        const size_t segments = (length / 6U) + 1U;
        for (size_t segment = 0; segment < segments; segment++) {
          std::vector<uint8_t> frame(8, 0xFF);
          frame[0] = channel;
          frame[1] = (uint8_t) segment;
          for (size_t i = 0; i < 6; i++) {
            const size_t index = segment * 6U + i;
            if (index < length) frame[i + 2] = (uint8_t) label[index];
          }
          const bool final = channel == 33 && segment + 1 == segments;
          this->queue_configuration_frame_(dgn, 0x17FD0400UL, frame, final);
          frame_count++;
        }
      }
    } else if (dgn == 0xFD06) {
      static const uint8_t frames[][8] = {
          {0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF},
          {0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF},
          {0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF},
          {0x17, 0x00, 0x1E, 0x00, 0x28, 0x00, 0xFF, 0xFF},
      };
      for (size_t i = 0; i < 4; i++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0600UL,
            std::vector<uint8_t>(frames[i], frames[i] + 8), i == 3);
        frame_count++;
      }
    } else if (dgn == 0xFD07) {
      static const uint8_t frames[][8] = {
          {0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
          {0x16, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
      };
      for (size_t i = 0; i < 2; i++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0700UL,
            std::vector<uint8_t>(frames[i], frames[i] + 8), i == 1);
        frame_count++;
      }
    } else if (dgn == 0xFD0A) {
      static const uint8_t frames[][8] = {
          {0x01,0x00,0x00,0x00,0x3B,0x00,0x01,0x00},
          {0x02,0x00,0x00,0x00,0x49,0x00,0x01,0x00},
          {0x03,0x00,0x00,0x00,0x47,0x00,0x01,0x00},
          {0x04,0x00,0x00,0x00,0x01,0x00,0x01,0x00},
          {0x05,0x00,0x00,0x00,0x00,0x00,0x01,0x00},
          {0x06,0x00,0x00,0x00,0x00,0x00,0x01,0x00},
          {0x07,0x00,0x00,0x00,0x00,0x00,0x01,0x00},
          {0x08,0x00,0x00,0x00,0x00,0x00,0x01,0x00},
          {0x09,0x0C,0x68,0x00,0x52,0x80,0x01,0x00},
          {0x0A,0x0C,0x68,0x00,0x46,0x81,0x01,0x00},
          {0x0B,0x08,0xFF,0xFF,0x00,0x00,0x01,0x00},
          {0x0C,0x0A,0xFF,0xFF,0x01,0x00,0x01,0x00},
          {0x0D,0x0A,0xFF,0xFF,0x52,0x12,0x01,0x00},
          {0x0E,0x0A,0xFF,0xFF,0x00,0x10,0x01,0x00},
          {0x0F,0x0A,0xFF,0xFF,0x48,0x00,0x01,0x00},
          {0x10,0x0A,0xFF,0xFF,0x44,0x00,0x01,0x00},
          {0x11,0x0A,0xFF,0xFF,0x3F,0x00,0x01,0x00},
          {0x12,0x0A,0xFF,0xFF,0x5E,0x00,0x01,0x00},
          {0x13,0x0A,0xFF,0xFF,0x1E,0x00,0x01,0x00},
          {0x14,0x0A,0xFF,0xFF,0x3E,0x00,0x01,0x00},
          {0x15,0x0A,0xFF,0xFF,0x16,0x00,0x01,0x00},
          {0x16,0x02,0x64,0x00,0x0B,0x00,0x01,0x00},
          {0x17,0x02,0x65,0x00,0x00,0x00,0x01,0x00},
          {0x18,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x19,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x1A,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x1B,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x1C,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x1D,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x1E,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x1F,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x20,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
          {0x21,0x0B,0xFF,0xFF,0x00,0x00,0x00,0x00},
      };
      for (size_t i = 0; i < 33; i++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0A00UL,
            std::vector<uint8_t>(frames[i], frames[i] + 8), i == 32);
        frame_count++;
      }
    } else if (dgn == 0xFD0C) {
      static const uint8_t frames[][8] = {
          {0x09,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
          {0x0A,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
          {0x16,0x61,0x00,0x00,0x00,0x00,0x60,0xEA},
          {0x17,0x61,0x00,0x00,0x00,0x00,0x60,0xEA},
      };
      for (size_t i = 0; i < 4; i++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0C00UL,
            std::vector<uint8_t>(frames[i], frames[i] + 8), i == 3);
        frame_count++;
      }
    } else if (dgn == 0xFD0E) {
      static const uint8_t frames[][8] = {
          {0x0C,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x0D,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x0E,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x0F,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x10,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x11,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x12,0x83,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x13,0x01,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x14,0x03,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
          {0x15,0x03,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
      };
      for (size_t i = 0; i < 10; i++) {
        this->queue_configuration_frame_(
            dgn, 0x17FD0E00UL,
            std::vector<uint8_t>(frames[i], frames[i] + 8), i == 9);
        frame_count++;
      }
    } else if (dgn == 0xFD10) {
      for (uint8_t channel = 1; channel <= 8; channel++) {
        const bool final = channel == 8;
        this->queue_configuration_frame_(
            dgn, 0x17FD1000UL,
            {channel, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
            final);
        frame_count++;
      }
    }

    if (frame_count == 0) {
      this->configuration_dgn_pending_[key] = false;
      return;
    }

    ESP_LOGI("redarc_tvms_rogue_emulator",
             "Queued %u captured frames for 0x1%04X configuration readback",
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
