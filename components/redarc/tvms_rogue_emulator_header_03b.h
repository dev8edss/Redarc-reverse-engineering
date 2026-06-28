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
