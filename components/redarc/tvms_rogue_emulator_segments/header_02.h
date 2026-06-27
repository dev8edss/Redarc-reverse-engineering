
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
