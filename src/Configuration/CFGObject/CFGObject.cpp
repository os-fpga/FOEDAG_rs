/*
Copyright 2022 The Foedag team

GPL License

Copyright (c) 2022 The Open-Source FPGA Foundation

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <fstream>
#include <iostream>

#include "CFGObject_auto.h"
#include "stdio.h"

#define OPTIMIZE_DATA_LENGTH

// Public functions
void CFGObject::write_bool(const std::string& name, bool value) const {
  write_data(name, "bool", value);
}

void CFGObject::write_u8(const std::string& name, uint8_t value) const {
  write_data(name, "u8", value);
}

void CFGObject::write_u16(const std::string& name, uint16_t value) const {
  write_data(name, "u16", value);
}

void CFGObject::write_u32(const std::string& name, uint32_t value) const {
  write_data(name, "u32", value);
}

void CFGObject::write_u64(const std::string& name, uint64_t value) const {
  write_data(name, "u64", value);
}

void CFGObject::write_i32(const std::string& name, int32_t value) const {
  write_data(name, "i32", value);
}

void CFGObject::write_i64(const std::string& name, int64_t value) const {
  write_data(name, "i64", value);
}

void CFGObject::write_u8s(const std::string& name,
                          std::vector<uint8_t> value) const {
  write_data(name, "u8s", value);
}

void CFGObject::write_u16s(const std::string& name,
                           std::vector<uint16_t> value) const {
  write_data(name, "u16s", value);
}

void CFGObject::write_u32s(const std::string& name,
                           std::vector<uint32_t> value) const {
  write_data(name, "u32s", value);
}

void CFGObject::write_u64s(const std::string& name,
                           std::vector<uint64_t> value) const {
  write_data(name, "u64s", value);
}

void CFGObject::write_i32s(const std::string& name,
                           std::vector<int32_t> value) const {
  write_data(name, "i32s", value);
}

void CFGObject::write_i64s(const std::string& name,
                           std::vector<int64_t> value) const {
  write_data(name, "i64s", value);
}

void CFGObject::write_str(const std::string& name,
                          const std::string& value) const {
  write_data(name, "str", value);
}

void CFGObject::append_u8s(const std::string& name,
                           std::vector<uint8_t> value) const {
  append_datas(name, "u8s", value);
}

void CFGObject::append_u16s(const std::string& name,
                            std::vector<uint16_t> value) const {
  append_datas(name, "u16s", value);
}

void CFGObject::append_u32s(const std::string& name,
                            std::vector<uint32_t> value) const {
  append_datas(name, "u32s", value);
}

void CFGObject::append_u64s(const std::string& name,
                            std::vector<uint64_t> value) const {
  append_datas(name, "u64s", value);
}

void CFGObject::append_i32s(const std::string& name,
                            std::vector<int32_t> value) const {
  append_datas(name, "i32s", value);
}

void CFGObject::append_i64s(const std::string& name,
                            std::vector<int64_t> value) const {
  append_datas(name, "i64s", value);
}

void CFGObject::append_str(const std::string& name,
                           const std::string& value) const {
  append_datas(name, "str", value);
}

void CFGObject::append_u8(const std::string& name, uint8_t value) const {
  append_data(name, "u8s", value);
}

void CFGObject::append_u16(const std::string& name, uint16_t value) const {
  append_data(name, "u16s", value);
}

void CFGObject::append_u32(const std::string& name, uint32_t value) const {
  append_data(name, "u32s", value);
}

void CFGObject::append_u64(const std::string& name, uint64_t value) const {
  append_data(name, "u64s", value);
}

void CFGObject::append_i32(const std::string& name, int32_t value) const {
  append_data(name, "i32s", value);
}

void CFGObject::append_i64(const std::string& name, int64_t value) const {
  append_data(name, "i64s", value);
}

void CFGObject::append_char(const std::string& name, char value) const {
  const CFGObject_RULE* rule = get_rule(name);
  CFG_ASSERT(rule->type == "str");
  std::string* ptr =
      reinterpret_cast<std::string*>(const_cast<void*>(rule->ptr));
  ptr->push_back(value);
  update_exist(rule);
}

bool CFGObject::write(const std::string& path) {
  // Only allow writing data at top level
  CFG_ASSERT(parent_ptr == nullptr);
  CFG_ASSERT(name.size() >= 2 && name.size() <= 8);
  error_msgs.clear();

  // Make sure all the rule meet
  bool status = false;
  if (status = check_rule(error_msgs)) {
    // Serialize data (fixed 8 bytes)
    std::vector<uint8_t> data;
    for (auto c : name) {
      data.push_back((uint8_t)(c));
    }
    while (data.size() < 8) {
      data.push_back(0);
    }

    //  Figure total object
    uint64_t object_count = get_object_count();
    write_variable_u64(data, object_count);

    // Serialize
    serialize(data);

    // Open file as binary and dump data
    std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);
    CFG_ASSERT(file.is_open());
    file.write((char*)(&data[0]), data.size());
    file.flush();
    file.close();
  }
  return status;
}

bool CFGObject::read(const std::string& path) {
  // Only allow reading data at top level
  CFG_ASSERT(parent_ptr == nullptr);
  CFG_ASSERT(name.size() >= 2 && name.size() <= 8);
  error_msgs.clear();

  // The class should be started from a blank one
  uint64_t object_count = get_object_count();
  CFG_ASSERT(object_count == 0);

  // File size to prepare memory
  std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
  CFG_ASSERT(file.is_open());
  size_t filesize = file.tellg();
  file.close();
  CFG_ASSERT(filesize >=
             11);  // at least 8 bytes name, 1 byte total object, 1 byte object
                   // (smallest bool or uint8_t), 1 byte value

  // Read the binary
  std::vector<uint8_t> data(filesize);
  file = std::ifstream(path.c_str(), std::ios::in | std::ios::binary);
  CFG_ASSERT(file.is_open());
  file.read((char*)(&data[0]), data.size());
  file.close();

  // Make sure filename is good
  size_t index = 0;
  std::string filename = get_string(&data[0], data.size(), index, 8, 2, 8);
  CFG_ASSERT(filename == name);

  // Get the object count
  object_count = get_variable_u64(&data[0], data.size(), index, 10);
  CFG_ASSERT(index < data.size());

  // Parse
  size_t parsed_object_count = 0;
  while (index < data.size()) {
    parse_object(&data[0], data.size(), index, parsed_object_count);
  }
  CFG_ASSERT(index == data.size());
  CFG_ASSERT((uint64_t)(parsed_object_count) == object_count);

  // Check rule
  return check_rule(error_msgs);
}

// Generic, Helper
const CFGObject_RULE* CFGObject::get_rule(const std::string& name) const {
  const CFGObject_RULE* rule = nullptr;
  for (auto& r : rules) {
    if (r.name == name) {
      rule = &r;
      break;
    }
  }
  CFG_ASSERT(rule != nullptr);
  return rule;
}

const CFGObject_RULE* CFGObject::get_rule(const void* ptr) const {
  const CFGObject_RULE* rule = nullptr;
  for (auto& r : rules) {
    if (r.ptr == ptr) {
      rule = &r;
      break;
    }
  }
  CFG_ASSERT(rule != nullptr);
  return rule;
}

void CFGObject::update_exist(const CFGObject_RULE* rule) const {
  if (!rule->is_exist) {
    rule->set_exist(true);
    if (parent_ptr != nullptr) {
      const CFGObject_RULE* rule = parent_ptr->get_rule(this);
      parent_ptr->update_exist(rule);
    }
  }
}

void CFGObject::update_exist(const std::string& name) const {
  const CFGObject_RULE* rule = CFGObject::get_rule(name);
  update_exist(rule);
}

bool CFGObject::check_rule(std::vector<std::string>& errors) const {
  // Currently only support one ruleis_exist
  return check_exist(errors);
}

bool CFGObject::check_exist(const std::string& name) const {
  const CFGObject_RULE* rule = get_rule(name);
  return rule->is_exist;
}

bool CFGObject::check_exist(std::vector<std::string>& errors) const {
  bool status = true;
  // Must follow the rule
  for (auto& r : rules) {
    // If the rule said this member must exist then we further check the
    // existence
    if (r.exist && !r.is_exist) {
      errors.push_back(CFG_print("%s does not exist", r.name.c_str()));
      status = false;
    }
    // Check if it has child
    if (r.type == "list") {
      const std::vector<CFGObject*>* ptr =
          reinterpret_cast<const std::vector<CFGObject*>*>(r.ptr);
      for (auto child_ptr : *ptr) {
        status = child_ptr->check_exist(errors) && status;
      }
    } else if (r.type == "class") {
      if (r.is_exist) {
        // For a class, only if the parent exist, we need to check the child
        const CFGObject* ptr = reinterpret_cast<const CFGObject*>(r.ptr);
        status = ptr->check_exist(errors) && status;
      }
    }
  }
  return status;
}

uint8_t CFGObject::get_type_enum(const std::string& type) const {
  auto iter = std::find(SUPPORTED_TYPES.begin(), SUPPORTED_TYPES.end(), type);
  CFG_ASSERT(iter != SUPPORTED_TYPES.end());
  size_t distance = std::distance(SUPPORTED_TYPES.begin(), iter);
  CFG_ASSERT(distance < 196);  // The rest reserved
  return (uint8_t)(distance);
}

uint64_t CFGObject::get_object_count() const {
  uint64_t count = 0;
  bool status = true;
  // Must follow the rule
  for (auto& r : rules) {
    // If the rule said this member must exist then we further check the
    // existence
    if (r.type == "list") {
      // For list, as long as the size() > 0, it exists
      const std::vector<CFGObject*>* ptr =
          reinterpret_cast<const std::vector<CFGObject*>*>(r.ptr);
      if (ptr->size()) {
        // list exist
        count++;
        // further loop each object in the list
        const std::vector<CFGObject*>* ptr =
            reinterpret_cast<const std::vector<CFGObject*>*>(r.ptr);
        for (auto child_ptr : *ptr) {
          count += child_ptr->get_object_count();
        }
      }
    } else if (r.is_exist) {
      // exists
      count++;
      if (r.type == "class") {
        // if this is a class, further check the child
        const CFGObject* ptr = reinterpret_cast<const CFGObject*>(r.ptr);
        count += ptr->get_object_count();
      }
    }
  }
  return count;
}

// Write
void CFGObject::serialize(std::vector<uint8_t>& data) const {
  // Only serialize those that exists
  for (auto& r : rules) {
    // Check if it has child
    if (r.type == "list") {
      const std::vector<CFGObject*>* ptr =
          reinterpret_cast<const std::vector<CFGObject*>*>(r.ptr);
      if (ptr->size()) {
        serialize_type_and_name(data, &r);
        write_variable_u64(data, (uint64_t)(ptr->size()));
        for (auto child_ptr : *ptr) {
          child_ptr->serialize(data);
          data.push_back(0xFF);
        }
      }
    } else if (r.is_exist) {
      serialize_type_and_name(data, &r);
      if (r.type == "class") {
        const CFGObject* ptr = reinterpret_cast<const CFGObject*>(r.ptr);
        ptr->serialize(data);
        data.push_back(0xFF);
      } else {
        serialize_value(data, &r);
      }
    }
  }
}

void CFGObject::serialize_type_and_name(std::vector<uint8_t>& data,
                                        const CFGObject_RULE* rule) const {
  CFG_ASSERT(rule != nullptr);
  CFG_ASSERT(rule->name.size() >= 2 && rule->name.size() <= 16);
  uint8_t type_enum = get_type_enum(rule->type);
  data.push_back(type_enum);
  for (auto c : rule->name) {
    data.push_back((uint8_t)(c));
  }
  if (rule->name.size() < 16) {
    data.push_back(0);
  }
}

void CFGObject::serialize_value(std::vector<uint8_t>& data,
                                const CFGObject_RULE* rule) const {
  CFG_ASSERT(rule != nullptr);
  void* ptr = const_cast<void*>(rule->ptr);
  if (rule->type == "bool") {
    serialize_data(data, *(reinterpret_cast<bool*>(ptr)));
  } else if (rule->type == "u8") {
    serialize_data(data, *(reinterpret_cast<uint8_t*>(ptr)));
  } else if (rule->type == "u16") {
    serialize_data(data, *(reinterpret_cast<uint16_t*>(ptr)));
  } else if (rule->type == "u32") {
    serialize_data(data, *(reinterpret_cast<uint32_t*>(ptr)));
  } else if (rule->type == "u64") {
    serialize_data(data, *(reinterpret_cast<uint64_t*>(ptr)));
  } else if (rule->type == "i32") {
    serialize_data(data, *(reinterpret_cast<int32_t*>(ptr)));
  } else if (rule->type == "i64") {
    serialize_data(data, *(reinterpret_cast<int64_t*>(ptr)));
  } else if (rule->type == "u8s") {
    serialize_datas(data, *(reinterpret_cast<std::vector<uint8_t>*>(ptr)));
  } else if (rule->type == "u16s") {
    serialize_datas(data, *(reinterpret_cast<std::vector<uint16_t>*>(ptr)));
  } else if (rule->type == "u32s") {
    serialize_datas(data, *(reinterpret_cast<std::vector<uint32_t>*>(ptr)));
  } else if (rule->type == "u64s") {
    serialize_datas(data, *(reinterpret_cast<std::vector<uint64_t>*>(ptr)));
  } else if (rule->type == "i32s") {
    serialize_datas(data, *(reinterpret_cast<std::vector<int32_t>*>(ptr)));
  } else if (rule->type == "i64s") {
    serialize_datas(data, *(reinterpret_cast<std::vector<int64_t>*>(ptr)));
  } else if (rule->type == "str") {
    std::string* string = reinterpret_cast<std::string*>(ptr);
    for (auto c : *string) {
      data.push_back((uint8_t)(c));
    }
    data.push_back(0);
  } else {
    CFG_INTERNAL_ERROR("serialize_value(): Unsupported type %s",
                       rule->type.c_str());
  }
}

uint8_t CFGObject::write_variable_u64(std::vector<uint8_t>& data,
                                      uint64_t value) const {
  // At least one byte
  uint8_t count = 1;
  data.push_back(value & 0x7F);
  value >>= 7;
  while (value) {
    CFG_ASSERT(count < 10);

    // Set MSB bit
    data.back() |= 0x80;

    // Put new value
    data.push_back(value & 0x7F);
    value >>= 7;
    count++;
  }
  return count;
}

// Read
void CFGObject::parse_object(const uint8_t* data, size_t data_size,
                             size_t& index, size_t& object_count) const {
  CFG_ASSERT(data != nullptr && data_size > 0);
  CFG_ASSERT(index < data_size);
  CFG_ASSERT(data[index] < (uint8_t)(SUPPORTED_TYPES.size()));
  std::string object_type = SUPPORTED_TYPES[data[index]];
  index++;
  CFG_ASSERT(index < data_size);
  std::string object_name = get_string(data, data_size, index, 16, 2);
  const CFGObject_RULE* rule = get_rule(object_name);
  CFG_ASSERT(rule->type == object_type);
  if (object_type == "bool") {
    read_data(data, data_size, index, rule, bool(false));
  } else if (object_type == "u8") {
    read_data(data, data_size, index, rule, uint8_t(0));
  } else if (object_type == "u16") {
    read_data(data, data_size, index, rule, uint16_t(0));
  } else if (object_type == "u32") {
    read_data(data, data_size, index, rule, uint32_t(0));
  } else if (object_type == "u64") {
    read_data(data, data_size, index, rule, uint64_t(0));
  } else if (object_type == "i32") {
    read_data(data, data_size, index, rule, int32_t(0));
  } else if (object_type == "i64") {
    read_data(data, data_size, index, rule, int64_t(0));
  } else if (object_type == "u8s") {
    read_datas(data, data_size, index, rule, uint8_t(0));
  } else if (object_type == "u16s") {
    read_datas(data, data_size, index, rule, uint16_t(0));
  } else if (object_type == "u32s") {
    read_datas(data, data_size, index, rule, uint32_t(0));
  } else if (object_type == "u64s") {
    read_datas(data, data_size, index, rule, uint64_t(0));
  } else if (object_type == "i32s") {
    read_datas(data, data_size, index, rule, int32_t(0));
  } else if (object_type == "i64s") {
    read_datas(data, data_size, index, rule, int64_t(0));
  } else if (object_type == "str") {
    write_data(rule, get_string(data, data_size, index));
  } else if (object_type == "class") {
    const CFGObject* ptr = reinterpret_cast<const CFGObject*>(rule->ptr);
    ptr->parse_class_object(data, data_size, index, object_count);
  } else if (object_type == "list") {
    uint64_t list_count = get_variable_u64(data, data_size, index, 10);
    std::vector<CFGObject*>* list = reinterpret_cast<std::vector<CFGObject*>*>(
        const_cast<void*>(rule->ptr));
    for (uint64_t i = 0; i < list_count; i++) {
      CFGObject_create_child_from_names(name, object_name,
                                        const_cast<CFGObject*>(this));
      list->back()->parse_class_object(data, data_size, index, object_count);
    }
  } else {
    CFG_INTERNAL_ERROR("parse_object(): Unsupport type %s",
                       object_type.c_str());
  }
  object_count++;
}

void CFGObject::parse_class_object(const uint8_t* data, size_t data_size,
                                   size_t& index, size_t& object_count) const {
  CFG_ASSERT(data != nullptr && data_size > 0);
  CFG_ASSERT(index < data_size);
  CFG_ASSERT(data[index] != 0xFF);
  while (data[index] != 0xFF) {
    CFG_ASSERT(index < data_size);
    parse_object(data, data_size, index, object_count);
  }
  CFG_ASSERT(index < data_size);
  CFG_ASSERT(data[index] == 0xFF);
  index++;
}

std::string CFGObject::get_string(const uint8_t* data, size_t data_size,
                                  size_t& index, int max_size, int min_size,
                                  int null_check) const {
  CFG_ASSERT(data != nullptr && data_size > 0);
  std::string string = "";
  int current_index = 0;
  while (1) {
    CFG_ASSERT(index < data_size);
    current_index++;
    if (data[index]) {
      string.push_back(char(data[index]));
      index++;
    } else {
      index++;
      break;
    }
    if (max_size != -1 && current_index == max_size) {
      break;
    }
  }
  CFG_ASSERT(min_size == -1 || int(string.size()) >= min_size);
  if (null_check != -1) {
    CFG_ASSERT(current_index <= null_check);
    while (current_index < null_check) {
      CFG_ASSERT(index < data_size);
      CFG_ASSERT(data[index] == 0);
      index++;
      current_index++;
    }
  }
  return string;
}

uint64_t CFGObject::get_variable_u64(const uint8_t* data, size_t data_size,
                                     size_t& index, int max_size) const {
  CFG_ASSERT(data != nullptr && data_size > 0);
  uint64_t u64 = 0;
  int current_index = 0;
  while (1) {
    // maximum is uint64_t (10 bytes which including next bit)
    CFG_ASSERT(current_index < 10);
    CFG_ASSERT(index < data_size);
    u64 |= (uint64_t)((uint64_t)(data[index] & 0x7F) << (current_index * 7));
    index++;
    current_index++;
    if ((data[index - 1] & 0x80) == 0) {
      break;
    }
  }
  CFG_ASSERT(max_size == -1 || current_index <= max_size);
  return u64;
}

// Template
template <typename T>
void CFGObject::write_data(const CFGObject_RULE* rule, T value) const {
  T* ptr = reinterpret_cast<T*>(const_cast<void*>(rule->ptr));
  (*ptr) = value;
  update_exist(rule);
}

template <typename T>
void CFGObject::write_data(const std::string& name, const std::string& type,
                           T value) const {
  const CFGObject_RULE* rule = get_rule(name);
  CFG_ASSERT(rule->type == type);
  write_data(rule, value);
}

template <typename T>
void CFGObject::read_data(const uint8_t* data, size_t data_size, size_t& index,
                          const CFGObject_RULE* rule, T value) const {
  deserialize_data(data, data_size, index, value);
  write_data(rule, value);
}

template <typename T>
void CFGObject::read_datas(const uint8_t* data, size_t data_size, size_t& index,
                           const CFGObject_RULE* rule, T value) const {
  std::vector<T> values;
  uint64_t list_count = get_variable_u64(data, data_size, index, 10);
  CFG_ASSERT(list_count > 0);
  for (uint64_t i = 0; i < list_count; i++) {
    deserialize_data(data, data_size, index, value);
    values.push_back(value);
  }
  write_data(rule, values);
}

template <typename T>
void CFGObject::append_datas(const std::string& name, const std::string& type,
                             T value) const {
  const CFGObject_RULE* rule = get_rule(name);
  CFG_ASSERT(rule->type == type);
  T* ptr = reinterpret_cast<T*>(const_cast<void*>(rule->ptr));
  ptr->insert(ptr->end(), value.begin(), value.end());
  update_exist(rule);
}

template <typename T>
void CFGObject::append_data(const std::string& name, const std::string& type,
                            T value) const {
  const CFGObject_RULE* rule = get_rule(name);
  CFG_ASSERT(rule->type == type);
  std::vector<T>* ptr =
      reinterpret_cast<std::vector<T>*>(const_cast<void*>(rule->ptr));
  ptr->push_back(value);
  update_exist(rule);
}

template <typename T>
void CFGObject::serialize_data(std::vector<uint8_t>& data, T value) const {
#if defined(OPTIMIZE_DATA_LENGTH)
  if (sizeof(T) == 1) {
    data.push_back((uint8_t)(value));
  } else if (sizeof(T) == 2) {
    uint8_t count = write_variable_u64(data, (uint64_t)(value)&0xFFFF);
    CFG_ASSERT(count <= 3);
  } else if (sizeof(T) == 4) {
    uint8_t count = write_variable_u64(data, (uint64_t)(value)&0xFFFFFFFF);
    CFG_ASSERT(count <= 5);
  } else if (sizeof(T) == 8) {
    uint8_t count = write_variable_u64(data, (uint64_t)(value));
    CFG_ASSERT(count <= 10);
  } else {
    CFG_INTERNAL_ERROR("Unsupported serialize_data T size");
  }
#else
  for (size_t i = 0; i < sizeof(T); i++) {
    data.push_back((uint8_t)(value));
    value >>= 8;
  }
#endif
}

template <typename T>
void CFGObject::deserialize_data(const uint8_t* data, size_t data_size,
                                 size_t& index, T& value) const {
  CFG_ASSERT(data != nullptr && data_size > 0);
  CFG_ASSERT(index < data_size);
#if defined(OPTIMIZE_DATA_LENGTH)
  if (sizeof(T) == 1) {
    value = (T)(data[index]);
    index++;
  } else {
    value = (T)(get_variable_u64(data, data_size, index,
                                 int(sizeof(T) + (sizeof(T) == 8 ? 2 : 1))));
  }
#else
  value = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    value |= (((uint64_t)(data[index]) & 0xFF) << (i * 8));
  }
#endif
}

template <typename T>
void CFGObject::serialize_datas(std::vector<uint8_t>& data,
                                std::vector<T>& value) const {
  write_variable_u64(data, (uint64_t)(value.size()));
  for (auto v : value) {
    serialize_data(data, v);
  }
}
