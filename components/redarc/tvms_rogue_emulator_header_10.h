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
