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
