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
      uint32_t active_tagged = 0;
      uint32_t tagged_value = 0;
      if (!this->channel_record_(channel, &record) ||
          !this->object_map_value_(record, 0x02U, &active_tagged) ||
          !this->object_map_value_(record, 0x03U, &tagged_value)) {
        return false;
      }
      const uint32_t value = this->decode_tagged_value_(tagged_value);
      const uint16_t active =
          (uint16_t) this->decode_tagged_value_(active_tagged);

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

      frames[channel - 1U] = {
          channel, channel_type,
          (uint8_t) (subtype & 0xFFU), (uint8_t) ((subtype >> 8) & 0xFFU),
          (uint8_t) (value & 0xFFU), (uint8_t) ((value >> 8) & 0xFFU),
          (uint8_t) (active & 0xFFU), (uint8_t) ((active >> 8) & 0xFFU)};
    }

