      return false;
    }
    if (channel_table != nullptr) *channel_table = table;
    return true;
  }

  bool channel_record_(uint8_t channel, uint32_t *record) const {
    uint32_t table = 0;
    uint32_t channel_record = 0;
    if (!this->channel_table_(&table) ||
        !this->object_map_value_(table, channel, &channel_record)) {
      return false;
    }
    if (record != nullptr) *record = channel_record;
    return true;
  }

  bool send_load_disconnect_config_() {
    uint32_t rogue_root = 0;
    uint32_t settings = 0;
    uint32_t trigger_tagged = 0;
    uint32_t disconnect_tagged = 0;
    uint32_t reconnect_tagged = 0;
    uint32_t disconnect_soc_tagged = 0;
    uint32_t reconnect_soc_tagged = 0;
    if (!this->rogue_object_root_(&rogue_root) ||
        !this->object_map_value_(rogue_root, 0x04U, &settings) ||
        !this->object_map_value_(settings, 0x01U, &trigger_tagged) ||
        !this->object_map_value_(settings, 0x02U, &disconnect_tagged) ||
        !this->object_map_value_(settings, 0x03U, &reconnect_tagged) ||
        !this->object_map_value_(settings, 0x04U, &disconnect_soc_tagged) ||
        !this->object_map_value_(settings, 0x05U, &reconnect_soc_tagged)) {
      return false;
    }

    const uint32_t trigger = this->decode_tagged_value_(trigger_tagged);
    const uint32_t disconnect_mv = this->decode_tagged_value_(disconnect_tagged);
    const uint32_t reconnect_mv = this->decode_tagged_value_(reconnect_tagged);
    const uint32_t disconnect_soc = this->decode_tagged_value_(disconnect_soc_tagged);
    const uint32_t reconnect_soc = this->decode_tagged_value_(reconnect_soc_tagged);
    if (disconnect_mv > 0xFFFFU || reconnect_mv > 0xFFFFU ||
        disconnect_soc > 0xFFU || reconnect_soc > 0xFFU) {
      return false;
    }

    this->send_frame_(
        redarc_common::with_sa(0x13F10800UL, this->source_address_),
        {(uint8_t) (0xE0U | ((trigger & 0x07U) << 2)),
         (uint8_t) (disconnect_mv & 0xFFU),
         (uint8_t) ((disconnect_mv >> 8) & 0xFFU),
         (uint8_t) (reconnect_mv & 0xFFU),
         (uint8_t) ((reconnect_mv >> 8) & 0xFFU),
         (uint8_t) disconnect_soc,
         (uint8_t) reconnect_soc,
         0x00});
    return true;
  }

  void sync_identity_from_active_object_() {
    uint32_t rogue_root = 0;
