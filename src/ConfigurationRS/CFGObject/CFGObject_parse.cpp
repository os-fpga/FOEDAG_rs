#include "CFGObject.h"

// Declaration
static void CFGObject_parse_class_object(std::ofstream& file,
                                         const uint8_t* data, size_t data_size,
                                         size_t& index, size_t& object_count,
                                         std::string space, bool detail);
static void CFGObject_parse_list_object(std::ofstream& file,
                                        const uint8_t* data, size_t data_size,
                                        size_t& index, size_t& object_count,
                                        std::string space, bool detail);
static void CFGObject_parse_object(std::ofstream& file, const uint8_t* data,
                                   size_t data_size, size_t& index,
                                   size_t& object_count, std::string space,
                                   bool detail);

static void CFGObject_parse_class_object(std::ofstream& file,
                                         const uint8_t* data, size_t data_size,
                                         size_t& index, size_t& object_count,
                                         std::string space, bool detail) {
  CFG_ASSERT(index < data_size);
  CFG_ASSERT(data[index] != 0xFF);
  while (data[index] != 0xFF) {
    CFG_ASSERT(index < data_size);
    CFGObject_parse_object(file, data, data_size, index, object_count, space,
                           detail);
  }
  CFG_ASSERT(index < data_size);
  CFG_ASSERT(data[index] == 0xFF);
  index++;
}

static void CFGObject_parse_list_object(std::ofstream& file,
                                        const uint8_t* data, size_t data_size,
                                        size_t& index, size_t& object_count,
                                        std::string space, bool detail) {
  CFG_ASSERT(index < data_size);
  uint64_t list_length = CFG_read_variable_u64(data, data_size, index, 10);
  CFG_ASSERT(list_length);
  for (uint64_t i = 0; i < list_length; i++) {
    file << space.c_str() << "#" << i << "\n";
    CFGObject_parse_object(file, data, data_size, index, object_count,
                           space + "  ", detail);
  }
}

template <typename T>
static void CFGObject_print_hex(std::ofstream& file, std::vector<T> data,
                                std::string space, bool detail) {
  file << "\n";
  file << space.c_str() << "size: " << data.size() << "\n";
  if (data.size()) {
    CFG_print_hex(file, (uint8_t*)(&data[0]),
                  (uint64_t)(data.size() * sizeof(data[0])),
                  (uint8_t)(sizeof(data[0])), space + "  ", detail);
  }
}

static void CFGObject_parse_object(std::ofstream& file, const uint8_t* data,
                                   size_t data_size, size_t& index,
                                   size_t& object_count, std::string space,
                                   bool detail) {
  CFG_ASSERT(index < data_size);
  CFG_ASSERT(data[index] < uint8_t(SUPPORTED_TYPES.size()));
  std::string object_type = SUPPORTED_TYPES[data[index]];
  index++;
  CFG_ASSERT(index < data_size);
  std::string object_name =
      CFGObject::get_string(data, data_size, index, 16, 2);
  file << space.c_str() << object_name.c_str()
       << " (type: " << object_type.c_str() << ")";
  file.flush();
  uint8_t u8 = 0;
  uint16_t u16 = 0;
  uint32_t u32 = 0;
  uint64_t u64 = 0;
  int32_t i32 = 0;
  int64_t i64 = 0;
  if (object_type == "bool" || object_type == "u8") {
    CFGObject::deserialize_data(data, data_size, index, u8);
    if (object_type == "bool") {
      CFG_ASSERT(u8 == 0 || u8 == 1);
      file << (u8 == 0 ? "false" : "true") << "\n";
    } else {
      file << " - " << CFG_print("0x%02X\n", u8).c_str();
    }
  } else if (object_type == "u16") {
    CFGObject::deserialize_data(data, data_size, index, u16);
    file << " - " << CFG_print("0x%04X\n", u16).c_str();
  } else if (object_type == "u32") {
    CFGObject::deserialize_data(data, data_size, index, u32);
    file << " - " << CFG_print("0x%08X\n", u32).c_str();
  } else if (object_type == "i32") {
    CFGObject::deserialize_data(data, data_size, index, i32);
    file << " - " << CFG_print("0x%08X\n", i32).c_str();
  } else if (object_type == "u64") {
    CFGObject::deserialize_data(data, data_size, index, u64);
    file << " - " << CFG_print("0x%016lX\n", u64).c_str();
  } else if (object_type == "i64") {
    CFGObject::deserialize_data(data, data_size, index, i64);
    file << " - " << CFG_print("0x%016lX\n", i64).c_str();
  } else if (object_type == "u8s") {
    CFGObject_print_hex(file,
                        CFGObject::read_raw_datas(data, data_size, index, u8),
                        space + "  ", detail);
  } else if (object_type == "u16s") {
    CFGObject_print_hex(file,
                        CFGObject::read_raw_datas(data, data_size, index, u16),
                        space + "  ", detail);
  } else if (object_type == "u32s") {
    CFGObject_print_hex(file,
                        CFGObject::read_raw_datas(data, data_size, index, u32),
                        space + "  ", detail);
  } else if (object_type == "i32s") {
    CFGObject_print_hex(file,
                        CFGObject::read_raw_datas(data, data_size, index, i32),
                        space + "  ", detail);
  } else if (object_type == "u64s") {
    CFGObject_print_hex(file,
                        CFGObject::read_raw_datas(data, data_size, index, u64),
                        space + "  ", detail);
  } else if (object_type == "i64s") {
    CFGObject_print_hex(file,
                        CFGObject::read_raw_datas(data, data_size, index, i64),
                        space + "  ", detail);
  } else if (object_type == "str") {
    std::string string = CFGObject::get_string(data, data_size, index);
    file << " " << string.c_str() << "\n";
  } else if (object_type == "class") {
    file << "\n";
    CFGObject_parse_class_object(file, data, data_size, index, object_count,
                                 space + "  ", detail);
  } else {
    CFG_ASSERT(object_type == "list");
    CFGObject_parse_list_object(file, data, data_size, index, object_count,
                                space + "  ", detail);
  }
  object_count++;
}

std::string CFGObject::get_string(const uint8_t* data, size_t data_size,
                                  size_t& index, int max_size, int min_size,
                                  int null_check) {
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

void CFGObject::parse(const std::string& input_filepath,
                      const std::string& output_filepath, bool detail) {
  std::vector<uint8_t> input_data;
  CFG_read_binary_file(input_filepath, input_data);
  // at least 8 bytes name, 1 byte total object, 1 byte object (smallest bool or
  // uint8_t), 1 byte value, 2 bytes CRC
  CFG_ASSERT(input_data.size() >= 13);

  // Check CRC first
  uint16_t crc = CFG_crc16(&input_data[0], input_data.size() - 2);
  CFG_ASSERT(((crc >> 8) & 0xFF) == input_data.back());
  input_data.pop_back();
  CFG_ASSERT((crc & 0xFF) == input_data.back());
  input_data.pop_back();

  // Output file
  std::ofstream file;
  file.open(output_filepath);
  CFG_ASSERT(file.is_open());

  size_t index = 0;
  std::string object_name =
      get_string(&input_data[0], input_data.size(), index, 8, 2, 8);
  file << "CFGObject: " << object_name.c_str() << "\n";

  uint64_t object_count =
      CFG_read_variable_u64(&input_data[0], input_data.size(), index, 10);
  file << "  Total Object: " << object_count << "\n";
  CFG_ASSERT(index < input_data.size());

  file << "  Objects\n";
  uint64_t parsed_object_count = 0;
  while (index < input_data.size()) {
    CFGObject_parse_object(file, &input_data[0], input_data.size(), index,
                           parsed_object_count, "    ", detail);
    file.flush();
  }
  file.close();
  CFG_ASSERT(object_count == parsed_object_count);
}
