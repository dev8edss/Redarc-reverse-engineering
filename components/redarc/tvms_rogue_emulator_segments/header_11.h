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
  uint32_t next_load_disconnect_frame_ms_{0};

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
