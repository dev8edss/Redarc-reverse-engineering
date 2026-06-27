    if (!this->read_object_node_(map_offset, 0x03, &length) ||
        (length % 8U) != 0U) {
      return false;
    }
    for (size_t index = 0; index < length; index += 8U) {
      const size_t entry = (size_t) map_offset + 4U + index;
      if (this->read_object_u32_(entry) != key) continue;
      if (value != nullptr) *value = this->read_object_u32_(entry + 4U);
      return true;
    }
    return false;
  }

  bool object_vector_value_(uint32_t vector_offset, size_t index,
                            uint32_t *value) const {
    size_t length = 0;
    if (!this->read_object_node_(vector_offset, 0x02, &length) ||
        (index + 1U) * 4U > length) {
      return false;
    }
    if (value != nullptr) {
      *value = this->read_object_u32_((size_t) vector_offset + 4U + index * 4U);
    }
    return true;
  }

  bool object_string_(uint32_t string_offset, std::string *value) const {
    size_t length = 0;
    if (!this->read_object_node_(string_offset, 0x01, &length)) return false;
    if (value != nullptr) {
