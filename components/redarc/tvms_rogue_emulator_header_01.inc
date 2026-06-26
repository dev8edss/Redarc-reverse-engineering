    this->source_address_ = v;
    this->register_configuration_readback_listener_();
  }
  void set_identity_interval_ms(uint32_t v) { identity_interval_ms_ = v; }
  void set_status_interval_ms(uint32_t v) { status_interval_ms_ = v; }
  void set_random_update_interval_ms(uint32_t v) { random_update_interval_ms_ = v; }
  void set_randomize_inputs(bool v) { randomize_inputs_ = v; }
  void set_serial_prefix(uint32_t v) { serial_prefix_ = v; }
  void set_serial_suffix(uint16_t v) {
    (void) v;
    serial_suffix_ = 0x0013;
  }
  void set_device_subtype(uint8_t v) { device_subtype_ = v; }
  void add_version_record(uint16_t p, uint8_t a, uint8_t b, uint8_t i) {
    (void) p;
    (void) a;
    (void) b;
    (void) i;
  }
  void set_manufacturing_date(uint8_t d, uint8_t m, uint16_t y) {
    manufacturing_day_ = d;
    manufacturing_month_ = m;
    manufacturing_year_ = y;
  }
  void set_product_name(const std::string &v) { product_name_ = v; }
  void set_unique_identifier(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                             uint8_t e, uint8_t f, uint8_t g) {
    unique_identifier_ = {a, b, c, d, e, f, g};
  }
  void set_unique_identifier_record_index(uint8_t v) {
    unique_identifier_record_index_ = v;
  }

  void set_tank1_sensor(sensor::Sensor *v) { tank1_sensor_ = v; }
  void set_tank2_sensor(sensor::Sensor *v) { tank2_sensor_ = v; }
  void set_input_voltage_sensor(sensor::Sensor *v) { input_voltage_sensor_ = v; }
  void set_input_current_sensor(sensor::Sensor *v) { input_current_sensor_ = v; }
  void set_output_status_text_sensor(text_sensor::TextSensor *v) {
    output_status_text_sensor_ = v;
  }
  void set_level_sensor(uint8_t n, sensor::Sensor *v) {
    if (n >= 1 && n <= 10) level_sensors_[n] = v;
  }
  void set_input_sensor(uint8_t n, binary_sensor::BinarySensor *v) {
    if (n >= 1 && n <= 8) input_sensors_[n] = v;
  }
  void register_light(uint8_t n, TVMSRogueEmulatorLight *v) {
    if (n >= 1 && n <= 10) lights_[n] = v;
  }

  void set_output_from_home_assistant(uint8_t output, float percent);
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);

 protected:
  struct ConfigurationReadbackFrame {
    uint32_t base_can_id;
    std::vector<uint8_t> data;
    uint16_t dgn;
    bool final_frame;
  };
