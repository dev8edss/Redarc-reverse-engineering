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
