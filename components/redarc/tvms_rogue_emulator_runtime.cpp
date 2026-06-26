#include "tvms_rogue_emulator.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace redarc_tvms_rogue_emulator {

static const char *const RUNTIME_TAG = "redarc_tvms_rogue_emulator";

namespace {
static constexpr uint32_t ID_CHANNEL_STATUS = 0x1BFD0000UL;
static constexpr uint32_t ID_SENSOR_VALUES = 0x1BFD0200UL;
static constexpr uint32_t ID_ACTIVE_CHANNELS = 0x17FD0800UL;
static constexpr uint32_t ID_OUTPUT_CAPABILITIES = 0x17FD0E00UL;
static constexpr uint32_t ID_OUTPUT_LEVELS = 0x1BFD1200UL;
static constexpr uint32_t ID_OUTPUT_ACTIVITY = 0x1BFD1400UL;
static constexpr uint32_t ID_DIRECT_ACK_BASE = 0x0F040000UL;
static constexpr uint32_t ID_SERVICE_DATA_BASE = 0x02810000UL;
static constexpr uint32_t ID_SERVICE_TRAILER_BASE = 0x02840000UL;

static constexpr uint16_t SERVICE_DGN_REQUEST = 0x0F03;
static constexpr uint16_t SERVICE_DIRECT_COMMAND = 0x0F00;
static constexpr uint16_t SERVICE_LEGACY_DIM = 0x0F05;
static constexpr uint16_t SERVICE_OBJECT_PREFIX = 0x0E00;

static constexpr uint16_t REQUEST_DGN_1FD00 = 0xFD00;
static constexpr uint16_t REQUEST_DGN_1FD02 = 0xFD02;
static constexpr uint16_t REQUEST_DGN_1FD08 = 0xFD08;
static constexpr uint16_t REQUEST_DGN_1FD0E = 0xFD0E;
static constexpr uint16_t REQUEST_DGN_1FD12 = 0xFD12;
static constexpr uint16_t REQUEST_DGN_1FD14 = 0xFD14;

static constexpr uint8_t MAIN_CONFIGURATION_OBJECT = 0x02;
static constexpr uint8_t CHANNEL_MASTER = 0x0B;
static constexpr uint8_t CHANNEL_OUTPUT_1 = 0x0C;
static constexpr uint8_t CHANNEL_OUTPUT_10 = 0x15;

uint32_t next_random_value() {
  static uint32_t state = 0x6D2B79F5UL;
  state = state * 1664525UL + 1013904223UL;
  return state;
}

uint8_t clamp_percent(float value) {
  if (value <= 0.0f) return 0;
  if (value >= 100.0f) return 100;
  return (uint8_t) std::lround(value);
}
}  // namespace

light::LightTraits TVMSRogueEmulatorLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void TVMSRogueEmulatorLight::write_state(light::LightState *state) {
  if (this->parent_ == nullptr || this->publishing_feedback_) return;
  const bool on = state->remote_values.is_on();
  float brightness = state->remote_values.get_brightness();
  if (on && brightness <= 0.0f) brightness = 1.0f;
  this->parent_->set_output_from_home_assistant(
      this->output_number_, on ? brightness * 100.0f : 0.0f);
}

void TVMSRogueEmulatorLight::publish_level(float percent) {
  if (this->state_ == nullptr) return;
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;

  this->publishing_feedback_ = true;
  const bool on = percent > 0.5f;
  this->state_->current_values.set_color_mode(light::ColorMode::BRIGHTNESS);
  this->state_->remote_values.set_color_mode(light::ColorMode::BRIGHTNESS);
  this->state_->current_values.set_state(on);
  this->state_->remote_values.set_state(on);
  if (on) {
    const float brightness = percent / 100.0f;
    this->state_->current_values.set_brightness(brightness);
    this->state_->remote_values.set_brightness(brightness);
  }
  this->state_->publish_state();
  this->publishing_feedback_ = false;
}

void TVMSRogueActiveEmulatorComponent::setup() {
  if (!this->load_configuration_object_()) {
    ESP_LOGE(RUNTIME_TAG, "Failed to load captured Rogue object 2");
    this->mark_failed();
    return;
  }

  redarc_common::RedarcCanDispatcher::instance().add_listener(
      [this](uint32_t id, const std::vector<uint8_t> &data) {
        this->handle_can_frame(id, data);
      });

  this->publish_all_home_assistant_states_();
  this->send_identity_();
  this->send_all_status_("startup");
  ESP_LOGI(RUNTIME_TAG, "Active virtual Rogue started at source address 0x%02X",
           this->source_address_);

  this->set_interval("rogue_emulator_identity", this->identity_interval_ms_, [this]() {
    this->send_identity_();
    ESP_LOGI(RUNTIME_TAG, "Broadcast identity 0x1F404");
  });

  this->set_interval("rogue_emulator_status", this->status_interval_ms_, [this]() {
    this->send_all_status_("periodic");
  });

  if (this->random_update_interval_ms_ > 0) {
    this->set_interval("rogue_emulator_random", this->random_update_interval_ms_, [this]() {
      this->randomize_sensor_values_();
    });
  }
}

void TVMSRogueActiveEmulatorComponent::dump_config() {
  TVMSRogueEmulatorComponent::dump_config();
  ESP_LOGCONFIG(RUNTIME_TAG, "  Active output/status emulation: enabled");
  ESP_LOGCONFIG(RUNTIME_TAG, "  Active channel inventory DGN 0x1FD08: enabled");
  ESP_LOGCONFIG(RUNTIME_TAG, "  Padded object block reads: enabled");
  ESP_LOGCONFIG(RUNTIME_TAG, "  Status interval: %u ms", (unsigned) this->status_interval_ms_);
  ESP_LOGCONFIG(RUNTIME_TAG, "  Random sensor interval: %u ms",
                (unsigned) this->random_update_interval_ms_);
  ESP_LOGCONFIG(RUNTIME_TAG, "  Random digital inputs: %s",
                this->randomize_inputs_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(RUNTIME_TAG,
                "  Home Assistant entities: 10 lights, 10 levels, 8 inputs, 4 sensors");
}

void TVMSRogueEmulatorComponent::set_output_from_home_assistant(uint8_t output,
                                                                float percent) {
  this->set_output_level_(output, clamp_percent(percent), "Home Assistant");
}

void TVMSRogueEmulatorComponent::set_output_level_(uint8_t output, uint8_t percent,
                                                   const char *origin) {
  if (output < 1 || output > 10) {
    ESP_LOGI(RUNTIME_TAG, "%s requested invalid output %u; ignored",
             origin, (unsigned) output);
    return;
  }
  if (percent > 100) percent = 100;

  const bool changed = this->output_levels_[output] != percent;
  this->output_levels_[output] = percent;
  if (this->level_sensors_[output] != nullptr)
    this->level_sensors_[output]->publish_state((float) percent);
  if (this->lights_[output] != nullptr)
    this->lights_[output]->publish_level((float) percent);

  ESP_LOGI(RUNTIME_TAG, "%s set Rogue output %u (channel 0x%02X) to %u%%%s",
           origin, (unsigned) output, (unsigned) (0x0B + output),
           (unsigned) percent, changed ? "" : " (unchanged)");

  this->publish_output_status_();
  this->send_channel_status_();
  this->send_output_levels_();
  this->send_output_activity_();
}

void TVMSRogueEmulatorComponent::set_master_state_(bool state, const char *origin) {
  this->master_state_ = state;
  ESP_LOGI(RUNTIME_TAG, "%s set Rogue master channel 0x0B %s",
           origin, state ? "ON" : "OFF");

  if (!state) {
    for (uint8_t output = 1; output <= 10; output++) {
      if (this->output_levels_[output] == 0) continue;
      this->output_levels_[output] = 0;
      if (this->level_sensors_[output] != nullptr)
        this->level_sensors_[output]->publish_state(0.0f);
      if (this->lights_[output] != nullptr)
        this->lights_[output]->publish_level(0.0f);
      ESP_LOGI(RUNTIME_TAG, "Master OFF forced output %u OFF", (unsigned) output);
    }
  }

  this->publish_output_status_();
  this->send_channel_status_();
  this->send_output_levels_();
}

void TVMSRogueEmulatorComponent::set_input_state_(uint8_t input, bool state,
                                                  const char *origin) {
  if (input < 1 || input > 8) return;
  this->input_states_[input] = state;
  if (this->input_sensors_[input] != nullptr)
    this->input_sensors_[input]->publish_state(state);
  ESP_LOGI(RUNTIME_TAG, "%s set Rogue digital input %u %s",
           origin, (unsigned) input, state ? "ON" : "OFF");
}

void TVMSRogueEmulatorComponent::randomize_sensor_values_() {
  this->tank1_percent_ = (uint8_t) (next_random_value() % 101U);
  this->tank2_percent_ = (uint8_t) (next_random_value() % 101U);
  this->input_voltage_mv_ = (uint16_t) (12000U + (next_random_value() % 2801U));
  this->input_current_ma_ = (uint16_t) (next_random_value() % 20001U);

  if (this->tank1_sensor_ != nullptr)
    this->tank1_sensor_->publish_state((float) this->tank1_percent_);
  if (this->tank2_sensor_ != nullptr)
    this->tank2_sensor_->publish_state((float) this->tank2_percent_);
  if (this->input_voltage_sensor_ != nullptr)
    this->input_voltage_sensor_->publish_state(this->input_voltage_mv_ / 1000.0f);
  if (this->input_current_sensor_ != nullptr)
    this->input_current_sensor_->publish_state(this->input_current_ma_ / 1000.0f);

  ESP_LOGI(RUNTIME_TAG,
           "Random sensors: tank1=%u%% tank2=%u%% voltage=%.3fV current=%.3fA",
           (unsigned) this->tank1_percent_, (unsigned) this->tank2_percent_,
           this->input_voltage_mv_ / 1000.0f, this->input_current_ma_ / 1000.0f);

  if (this->randomize_inputs_) {
    const uint8_t input = (uint8_t) ((next_random_value() % 8U) + 1U);
    this->set_input_state_(input, !this->input_states_[input], "Random generator");
  }

  this->send_sensor_values_();
  this->send_channel_status_();
}

void TVMSRogueEmulatorComponent::publish_all_home_assistant_states_() {
  if (this->tank1_sensor_ != nullptr)
    this->tank1_sensor_->publish_state((float) this->tank1_percent_);
  if (this->tank2_sensor_ != nullptr)
    this->tank2_sensor_->publish_state((float) this->tank2_percent_);
  if (this->input_voltage_sensor_ != nullptr)
    this->input_voltage_sensor_->publish_state(this->input_voltage_mv_ / 1000.0f);
  if (this->input_current_sensor_ != nullptr)
    this->input_current_sensor_->publish_state(this->input_current_ma_ / 1000.0f);

  for (uint8_t input = 1; input <= 8; input++) {
    if (this->input_sensors_[input] != nullptr)
      this->input_sensors_[input]->publish_state(this->input_states_[input]);
  }
  for (uint8_t output = 1; output <= 10; output++) {
    if (this->level_sensors_[output] != nullptr)
      this->level_sensors_[output]->publish_state((float) this->output_levels_[output]);
    if (this->lights_[output] != nullptr)
      this->lights_[output]->publish_level((float) this->output_levels_[output]);
  }
  this->publish_output_status_();
  ESP_LOGI(RUNTIME_TAG, "Published all virtual Rogue states to Home Assistant");
}

void TVMSRogueEmulatorComponent::publish_output_status_() {
  std::string summary;
  for (uint8_t output = 1; output <= 10; output++) {
    if (!summary.empty()) summary += ", ";
    summary += "O" + std::to_string(output) + "=" +
               std::to_string(this->output_levels_[output]) + "%";
  }
  if (summary == this->last_output_status_) return;
  this->last_output_status_ = summary;
  if (this->output_status_text_sensor_ != nullptr)
    this->output_status_text_sensor_->publish_state(summary);
  ESP_LOGI(RUNTIME_TAG, "Home Assistant output status: %s", summary.c_str());
}

void TVMSRogueEmulatorComponent::send_channel_status_() {
  std::vector<uint8_t> page1{0x01};
  for (uint8_t input = 1; input <= 7; input++)
    page1.push_back(this->input_states_[input] ? 0x01 : 0x00);
  this->send_frame_(redarc_common::with_sa(ID_CHANNEL_STATUS, this->source_address_), page1);

  std::vector<uint8_t> page8{
      0x08,
      (uint8_t) (this->input_states_[8] ? 0x01 : 0x00),
      0xFF, 0xFF,
      (uint8_t) (this->master_state_ ? 0x01 : 0x00),
      (uint8_t) (this->output_levels_[1] > 0 ? 0x01 : 0x00),
      (uint8_t) (this->output_levels_[2] > 0 ? 0x01 : 0x00),
      (uint8_t) (this->output_levels_[3] > 0 ? 0x01 : 0x00),
  };
  this->send_frame_(redarc_common::with_sa(ID_CHANNEL_STATUS, this->source_address_), page8);

  std::vector<uint8_t> page15{0x0F};
  for (uint8_t output = 4; output <= 10; output++)
    page15.push_back(this->output_levels_[output] > 0 ? 0x01 : 0x00);
  this->send_frame_(redarc_common::with_sa(ID_CHANNEL_STATUS, this->source_address_), page15);

  this->send_frame_(redarc_common::with_sa(ID_CHANNEL_STATUS, this->source_address_),
                    {0x18, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8});
  this->send_frame_(redarc_common::with_sa(ID_CHANNEL_STATUS, this->source_address_),
                    {0x1F, 0xF8, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF});
  ESP_LOGI(RUNTIME_TAG, "Broadcast 0x1FD00 channel status pages");
}

void TVMSRogueEmulatorComponent::send_output_levels_() {
  this->send_frame_(redarc_common::with_sa(ID_OUTPUT_LEVELS, this->source_address_),
                    {0x0C, output_levels_[1], output_levels_[2], output_levels_[3],
                     output_levels_[4], output_levels_[5], output_levels_[6], output_levels_[7]});
  this->send_frame_(redarc_common::with_sa(ID_OUTPUT_LEVELS, this->source_address_),
                    {0x13, output_levels_[8], output_levels_[9], output_levels_[10],
                     0xFF, 0xFF, 0x00, 0x00});
  this->send_frame_(redarc_common::with_sa(ID_OUTPUT_LEVELS, this->source_address_),
                    {0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  this->send_frame_(redarc_common::with_sa(ID_OUTPUT_LEVELS, this->source_address_),
                    {0x21, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  ESP_LOGI(RUNTIME_TAG, "Broadcast 0x1FD12 output levels");
}

void TVMSRogueEmulatorComponent::send_sensor_values_() {
  this->send_frame_(redarc_common::with_sa(ID_SENSOR_VALUES, this->source_address_),
                    {0x09, tank1_percent_, tank2_percent_, 0x00, 0x00, 0xFF, 0xFF, 0xFF});
  this->send_frame_(redarc_common::with_sa(ID_SENSOR_VALUES, this->source_address_),
                    {0x16,
                     (uint8_t) (input_voltage_mv_ & 0xFFU),
                     (uint8_t) ((input_voltage_mv_ >> 8) & 0xFFU),
                     (uint8_t) (input_current_ma_ & 0xFFU),
                     (uint8_t) ((input_current_ma_ >> 8) & 0xFFU),
                     0xFF, 0xFF, 0xFF});
  ESP_LOGI(RUNTIME_TAG, "Broadcast 0x1FD02 tank, voltage and current values");
}

void TVMSRogueEmulatorComponent::send_output_capabilities_() {
  for (uint8_t channel = CHANNEL_OUTPUT_1; channel <= CHANNEL_OUTPUT_10; channel++) {
    this->send_frame_(redarc_common::with_sa(ID_OUTPUT_CAPABILITIES, this->source_address_),
                      {channel, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  }
  ESP_LOGI(RUNTIME_TAG, "Broadcast 0x1FD0E: all 10 outputs are dimmable");
}

void TVMSRogueEmulatorComponent::send_output_activity_() {
  this->send_frame_(redarc_common::with_sa(ID_OUTPUT_ACTIVITY, this->source_address_),
                    {0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  this->send_frame_(redarc_common::with_sa(ID_OUTPUT_ACTIVITY, this->source_address_),
                    {0x13, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF});
  ESP_LOGI(RUNTIME_TAG, "Broadcast 0x1FD14 output activity");
}

void TVMSRogueEmulatorComponent::send_all_status_(const char *reason) {
  ESP_LOGI(RUNTIME_TAG, "Sending complete virtual Rogue status (%s)", reason);
  this->send_channel_status_();
  this->send_output_levels_();
  this->send_sensor_values_();
  this->send_output_activity_();
}

void TVMSRogueEmulatorComponent::send_direct_ack_(uint8_t requester, uint8_t command) {
  const uint32_t response_id = ID_DIRECT_ACK_BASE |
                               ((uint32_t) requester << 8) |
                               this->source_address_;
  this->send_frame_(response_id,
                    {0x01, command, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF});
  ESP_LOGI(RUNTIME_TAG, "Sent direct-command ACK for 0x%02X to requester 0x%02X",
           command, requester);
}

void TVMSRogueActiveEmulatorComponent::handle_can_frame(
    uint32_t can_id, const std::vector<uint8_t> &data) {
  const uint32_t id = redarc_common::rvc_id(can_id);
  const uint16_t service = (uint16_t) ((id >> 16) & 0xFFFFU);
  const uint8_t destination = (uint8_t) ((id >> 8) & 0xFFU);
  const uint8_t requester = (uint8_t) (id & 0xFFU);

  if (destination != this->source_address_) return;

  // The real Rogue always satisfies the requested 0E86 block length. When a
  // request extends past the declared object length, it pads the remainder with
  // 0xFF and calculates the block CRC across the complete requested block.
  // This matters for the final offset 0x1200 request: object 2 contains 64 bytes
  // there, but the app requests and expects a full 256-byte response.
  if ((service & 0xFF00U) == SERVICE_OBJECT_PREFIX &&
      (service & 0x00FFU) == 0x86U && data.size() >= 8 &&
      this->selected_object_ == MAIN_CONFIGURATION_OBJECT) {
    const uint32_t offset = redarc_common::u32_le(data, 0);
    const uint32_t requested_length = redarc_common::u32_le(data, 4);

    std::vector<uint8_t> block((size_t) requested_length, 0xFF);
    if (offset < this->configuration_object_.size()) {
      const size_t available = this->configuration_object_.size() - (size_t) offset;
      const size_t copy_length = std::min<size_t>(block.size(), available);
      std::copy_n(this->configuration_object_.begin() + offset, copy_length,
                  block.begin());
    }

    const uint32_t data_id = ID_SERVICE_DATA_BASE |
                             ((uint32_t) requester << 8) |
                             this->source_address_;
    for (size_t block_offset = 0; block_offset < block.size(); block_offset += 8) {
      std::vector<uint8_t> frame(8, 0xFF);
      const size_t chunk = std::min<size_t>(8, block.size() - block_offset);
      std::copy_n(block.begin() + block_offset, chunk, frame.begin());
      this->send_frame_(data_id, frame);
    }

    const uint32_t block_crc = this->crc32c_(
        block.empty() ? nullptr : block.data(), block.size());
    const uint32_t trailer_id = ID_SERVICE_TRAILER_BASE |
                                ((uint32_t) requester << 8) |
                                this->source_address_;
    this->send_frame_(
        trailer_id,
        {
            (uint8_t) (requested_length & 0xFFU),
            (uint8_t) ((requested_length >> 8) & 0xFFU),
            (uint8_t) ((requested_length >> 16) & 0xFFU),
            (uint8_t) ((requested_length >> 24) & 0xFFU),
            (uint8_t) (block_crc & 0xFFU),
            (uint8_t) ((block_crc >> 8) & 0xFFU),
            (uint8_t) ((block_crc >> 16) & 0xFFU),
            (uint8_t) ((block_crc >> 24) & 0xFFU),
        });

    ESP_LOGI(RUNTIME_TAG,
             "Padded object %u read offset=%lu requested=%lu returned=%lu",
             (unsigned) this->selected_object_, (unsigned long) offset,
             (unsigned long) requested_length, (unsigned long) block.size());
    return;
  }

  if (service == SERVICE_DGN_REQUEST && data.size() >= 2) {
    const uint16_t requested_dgn = (uint16_t) data[0] | ((uint16_t) data[1] << 8);
    switch (requested_dgn) {
      case REQUEST_DGN_1FD00:
        ESP_LOGI(RUNTIME_TAG, "Requester 0x%02X requested 0x1FD00", requester);
        this->send_channel_status_();
        return;
      case REQUEST_DGN_1FD02:
        ESP_LOGI(RUNTIME_TAG, "Requester 0x%02X requested 0x1FD02", requester);
        this->send_sensor_values_();
        return;
      case REQUEST_DGN_1FD08:
        ESP_LOGI(RUNTIME_TAG,
                 "Requester 0x%02X requested 0x1FD08 active-channel inventory",
                 requester);
        this->send_frame_(redarc_common::with_sa(ID_ACTIVE_CHANNELS, this->source_address_),
                          {0x21, 0xFF, 0xFF, 0x1E, 0xFF, 0xFF, 0xFF, 0xFF});
        ESP_LOGI(RUNTIME_TAG,
                 "Responded 0x1FD08: Rogue channels 0x01-0x21 with active mask 0x1E");
        return;
      case REQUEST_DGN_1FD0E:
        ESP_LOGI(RUNTIME_TAG, "Requester 0x%02X requested 0x1FD0E", requester);
        this->send_output_capabilities_();
        return;
      case REQUEST_DGN_1FD12:
        ESP_LOGI(RUNTIME_TAG, "Requester 0x%02X requested 0x1FD12", requester);
        this->send_output_levels_();
        return;
      case REQUEST_DGN_1FD14:
        ESP_LOGI(RUNTIME_TAG, "Requester 0x%02X requested 0x1FD14", requester);
        this->send_output_activity_();
        return;
      default:
        ESP_LOGI(RUNTIME_TAG, "Requester 0x%02X requested DGN 0x1%04X",
                 requester, requested_dgn);
        TVMSRogueEmulatorComponent::handle_can_frame(can_id, data);
        return;
    }
  }

  if (service == SERVICE_DIRECT_COMMAND && data.size() >= 5) {
    const uint8_t command = data[0];
    if (command == 0xCB && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      const bool state = data[4] != 0;
      if (channel == CHANNEL_MASTER) {
        this->set_master_state_(state, "CAN command 0xCB");
      } else if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) {
        const uint8_t output = channel - CHANNEL_MASTER;
        uint8_t level = state ? this->output_levels_[output] : 0;
        if (state && level == 0) level = 100;
        this->set_output_level_(output, level, "CAN command 0xCB");
      } else {
        ESP_LOGI(RUNTIME_TAG, "Ignored 0xCB for unsupported channel 0x%02X", channel);
      }
      this->send_direct_ack_(requester, command);
      return;
    }

    if (command == 0x5A && data[1] == 0x01 && data[2] == 0xFF) {
      const uint8_t channel = data[3];
      if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) {
        this->set_output_level_(channel - CHANNEL_MASTER,
                                std::min<uint8_t>(data[4], 100),
                                "CAN command 0x5A");
      } else {
        ESP_LOGI(RUNTIME_TAG, "Ignored 0x5A for unsupported channel 0x%02X", channel);
      }
      this->send_direct_ack_(requester, command);
      return;
    }

    ESP_LOGI(RUNTIME_TAG, "Ignored unsupported direct command 0x%02X from 0x%02X",
             command, requester);
    this->send_direct_ack_(requester, command);
    return;
  }

  if (service == SERVICE_LEGACY_DIM && data.size() >= 3) {
    const uint8_t channel = data[0];
    const uint8_t direction = data[2];
    if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) {
      const uint8_t output = channel - CHANNEL_MASTER;
      if (direction == 0x01) {
        const uint8_t level = this->output_levels_[output] > 5
                                  ? this->output_levels_[output] - 5 : 0;
        this->set_output_level_(output, level, "Legacy dim-down");
      } else if (direction == 0x64) {
        const uint8_t level = std::min<uint8_t>(100, this->output_levels_[output] + 5);
        this->set_output_level_(output, level, "Legacy dim-up");
      } else if (direction == 0xFF) {
        ESP_LOGI(RUNTIME_TAG, "Legacy dim release for output %u", (unsigned) output);
      } else {
        ESP_LOGI(RUNTIME_TAG, "Unknown legacy dim direction 0x%02X for output %u",
                 direction, (unsigned) output);
      }
    } else {
      ESP_LOGI(RUNTIME_TAG, "Ignored legacy dim for channel 0x%02X", channel);
    }
    return;
  }

  TVMSRogueEmulatorComponent::handle_can_frame(can_id, data);
}

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
