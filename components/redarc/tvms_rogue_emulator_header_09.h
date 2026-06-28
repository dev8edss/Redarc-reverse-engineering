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
      const bool dimmable = this->decode_tagged_value_(dim_tagged) != 0U;
      uint8_t capability = dimmable ? 0x83 : 0x01;
      if (!dimmable && this->decode_tagged_value_(switch_tagged) != 0U)
        capability |= 0x02;
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
    for (uint8_t channel = 1; channel <= 8; channel++) {
      this->queue_configuration_frame_(
          dgn, 0x17FD1000UL,
          {channel, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
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
