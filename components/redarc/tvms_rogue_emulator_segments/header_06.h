    this->serial_prefix_ = object_prefix;
    if (decoded_suffix <= 0xFFFFU)
      this->serial_suffix_ = (uint16_t) decoded_suffix;
    this->product_name_ = object_name;

    if (changed) {
      ESP_LOGI("redarc_tvms_rogue_emulator",
               "Active object identity: %010lu-%04u %s",
               (unsigned long) this->serial_prefix_,
               (unsigned) this->serial_suffix_, this->product_name_.c_str());
    }
  }

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
            case 0xF108:
              this->send_load_disconnect_config_();
              break;
            case 0xF403:
            case 0xF404:
              this->sync_identity_from_active_object_();
              break;
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

  bool queue_active_labels_(uint16_t dgn, size_t *frame_count) {
    std::array<std::string, 33> labels;
    for (uint8_t channel = 1; channel <= 33; channel++) {
