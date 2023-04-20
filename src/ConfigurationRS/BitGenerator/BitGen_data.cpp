#include "BitGen_data.h"

#include "CFGCrypto/CFGOpenSSL.h"

BitGen_DATA::BitGen_DATA(const std::string& n, const uint8_t s,
                         std::vector<BitGen_DATA_RULE> r)
    : name(n), size_alignment(s), rules(r) {
  CFG_ASSERT(name.size());
  CFG_ASSERT(rules.size());
}

BitGen_DATA::~BitGen_DATA() {
  while (rules.size()) {
    rules.pop_back();
  }
}

void BitGen_DATA::set_src(const std::string& name, uint64_t value) {
  CFG_ASSERT(srcs.find(name) == srcs.end());
  srcs[name] = BitGEN_SRC_DATA(value);
}

void BitGen_DATA::set_src(const std::string& name,
                          const std::vector<uint8_t>& data) {
  CFG_ASSERT(srcs.find(name) == srcs.end());
  srcs[name] = BitGEN_SRC_DATA(data);
}

void BitGen_DATA::set_src(const std::string& name, const uint8_t* data,
                          const size_t data_size) {
  CFG_ASSERT(data != nullptr && data_size > 0);
  std::vector<uint8_t> temp;
  for (size_t i = 0; i < data_size; i++) {
    temp.push_back(data[i]);
  }
  set_src(name, temp);
}

uint64_t BitGen_DATA::generate(std::vector<uint8_t>& data, bool& compress,
                               std::vector<uint8_t>& key, uint8_t* iv,
                               std::vector<std::string> binary_datas) {
  size_t retry = rules.size() * 2;
  bool complete = false;
  // set_data: outside world can overwrite anything -- highest priority
  for (auto& src : srcs) {
    BitGen_DATA_RULE* rule = get_rule(src.first);
    if (rule != nullptr) {
      set_data(rule, src.second.data);
    }
  }
  while (retry) {
    size_t incomplete = 0;
    uint64_t continuous_size = 0;
    for (auto& r : rules) {
      check_rule_size(&r, incomplete, continuous_size);
      if (r.size > 0 && r.data.size() == 0) {
        incomplete++;
        // set_data: use json definition -- second priority
        if (set_datas(r.name) == 0) {
          // set_data: use default value -- lowest priority
          set_data(&r, r.default_value);
        }
        if (r.data.size() != 0) {
          incomplete--;
        }
      }
    }
    if (incomplete == 0) {
      complete = true;
      break;
    }
    retry--;
  }
  CFG_ASSERT(complete);

  // compress and/or encrypt data
  if (compress || key.size() > 0) {
    CFG_ASSERT(key.size() == 0 || key.size() == 16 || key.size() == 32);
    CFG_ASSERT(key.size() == 0 || iv != nullptr);
    // Let's support only one continuous data
    if (binary_datas.size() == 0) {
      // caller does not explicitely tell which data to encrypt
      // so we get it from JSON
      for (auto& rule : rules) {
        if (rule.size != 0) {
          if (rule.property & 1) {
            binary_datas.push_back(rule.name);
          }
        }
      }
    }
    if (binary_datas.size()) {
      // Let's only support one field to compress (don't really care about
      // encryption) Otherwsie it is hard to determine the which field has what
      // size
      CFG_ASSERT(binary_datas.size() == 1);
      uint8_t tracking = 0;
      uint64_t pre_binary_size = 0;
      uint64_t binary_size = 0;
      for (auto& rule : rules) {
        if (rule.size != 0) {
          if (CFG_find_string_in_vector(binary_datas, rule.name) >= 0) {
            CFG_ASSERT(tracking == 0 || tracking == 1);
            if (tracking == 0) {
              CFG_ASSERT((pre_binary_size % 8) == 0);
              tracking++;
            }
            binary_size += rule.size;
          } else {
            // this is not binary data
            if (tracking == 0) {
              pre_binary_size += rule.size;
            } else if (tracking == 1) {
              tracking++;
            }
          }
        }
      }
      // No violation
      CFG_ASSERT(binary_size > 0 && (binary_size % 8) == 0);
      // Start to copy original data
      uint64_t binary_byte_size = (binary_size / 8);
      std::vector<uint8_t> final_data(binary_byte_size);
      std::vector<uint8_t> temp_data;
      uint64_t dest = 0;
      for (auto& rule : rules) {
        if (rule.size != 0) {
          if (CFG_find_string_in_vector(binary_datas, rule.name) >= 0) {
            for (uint64_t i = 0; i < rule.size; i++, dest++) {
              if (rule.data[i >> 3] & (1 << (i & 7))) {
                final_data[dest >> 3] |= (1 << (dest & 7));
              }
            }
          }
        }
      }
      CFG_ASSERT(dest == binary_size);
      // Compress
      if (compress) {
        CFG_compress(&final_data[0], final_data.size(), temp_data);
        CFG_ASSERT(temp_data.size());
        if (size_alignment) {
          CFG_ASSERT((size_alignment % 8) == 0);
          size_t byte_alignment = size_t(size_alignment / 8);
          // Padding
          while (temp_data.size() % byte_alignment) {
            if (key.size() > 0) {
              temp_data.push_back((uint8_t)(CFGOpenSSL::generate_random_u32()));
            } else {
              temp_data.push_back(0);
            }
          }
        }
        if (temp_data.size() >= (size_t)(final_data.size() * 0.90)) {
          compress = false;
        } else {
          memset(&final_data[0], 0, final_data.size());
          final_data.resize(temp_data.size());
          memcpy(&final_data[0], &temp_data[0], temp_data.size());
        }
        memset(&temp_data[0], 0, temp_data.size());
        temp_data.clear();
      }
      // Encrypt
      if (key.size() > 0) {
        temp_data.resize(final_data.size());
        CFGOpenSSL::ctr_encrypt(&final_data[0], &temp_data[0],
                                final_data.size(), &key[0], key.size(), iv, 16);
        memcpy(&final_data[0], &temp_data[0], temp_data.size());
        memset(&temp_data[0], 0, temp_data.size());
        temp_data.clear();
      }
      // Copy data back to the field
      if (compress || key.size() > 0) {
        dest = 0;
        for (auto& rule : rules) {
          if (rule.size != 0) {
            if (CFG_find_string_in_vector(binary_datas, rule.name) >= 0) {
              CFG_ASSERT(rule.data.size() >= final_data.size());
              memset(&rule.data[0], 0, rule.data.size());
              rule.size = final_data.size() * 8;
              rule.data.resize(final_data.size());
              for (uint64_t i = 0; i < rule.size; i++, dest++) {
                if (final_data[dest >> 3] & (1 << (dest & 7))) {
                  rule.data[i >> 3] |= (1 << (i & 7));
                }
              }
            }
          }
        }
      }
      memset(&final_data[0], 0, final_data.size());
      if (compress) {
        // Update any field that related to the binary size change
        for (auto& rule : rules) {
          uint32_t property = (rule.property >> 1) & 0x7;
          if (rule.size != 0 && property != 0) {
            CFG_ASSERT(rule.size <= 64);
            // 1 = size8 (1byte), 2 = size16 (2byte), 3 = size32 (4byte), 4 =
            // size64 (8byte), ...
            uint64_t divider = 1 << (property - 1);
            CFG_ASSERT((final_data.size() % divider) == 0);
            uint64_t new_data = final_data.size() / divider;
            memset(&rule.data[0], 0, rule.data.size());
            for (uint64_t i = 0; i < rule.size; i++) {
              if (new_data & (1 << i)) {
                rule.data[i >> 3] |= (1 << (i & 7));
              }
            }
          }
        }
      }
      final_data.clear();
    }
  }

  uint64_t total_bit_size = validate();
  // copy all data into one continuous piece
  uint64_t dest = 0;
  uint64_t total_size8 = convert_to8(total_bit_size);
  data.resize(total_size8);
  memset(&data[0], 0, total_size8);
  for (auto& rule : rules) {
    if (rule.size != 0) {
      for (uint64_t i = 0; i < rule.size; i++, dest++) {
        if (rule.data[i >> 3] & (1 << (i & 7))) {
          data[dest >> 3] |= (1 << (dest & 7));
        }
      }
    }
  }
  return total_bit_size;
}

uint64_t BitGen_DATA::get_header_size() {
  uint64_t header_size = 0;
  for (auto& rule : rules) {
    if (rule.size != 0) {
      if ((rule.property & 1) == 0) {
        header_size += rule.size;
      }
    } else {
      CFG_ASSERT(rule.info & 1);
    }
  }
  return header_size;
}

uint64_t BitGen_DATA::parse(std::ofstream& file, const uint8_t* data,
                            uint64_t total_bit_size, uint64_t& bit_index,
                            const bool compress, std::string space,
                            const bool detail) {
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(total_bit_size > 0);
  CFG_ASSERT(bit_index < total_bit_size);
  size_t retry = rules.size() * 2;
  bool complete = false;
  ignore_set_size_property = compress;
  while (retry) {
    size_t incomplete = 0;
    uint64_t continuous_size = 0;
    for (auto& r : rules) {
      check_rule_size(&r, incomplete, continuous_size);
      if (r.size > 0 && r.data.size() == 0 && continuous_size != uint64_t(-1)) {
        CFG_ASSERT((bit_index + r.size) <= total_bit_size);
        uint64_t u8 = convert_to8(r.size);
        r.data.resize(u8);
        memset(&r.data[0], 0, u8);
        for (uint64_t i = 0; i < r.size; i++, bit_index++) {
          if (data[bit_index >> 3] & (1 << (bit_index & 7))) {
            r.data[i >> 3] |= (1 << (i & 7));
          }
        }
      }
      if (r.size > 0 && r.data.size() == 0) {
        incomplete++;
      }
    }
    if (incomplete == 0) {
      complete = true;
      break;
    }
    retry--;
  }
  CFG_ASSERT(complete);
  ignore_set_size_property = false;
  return print(file, compress, space, detail);
}

uint64_t BitGen_DATA::print(std::ofstream& file, const bool compress,
                            std::string space, const bool detail) {
  // print_info
  //    0:      - nature printing
  //    1-5:    - alignment printing
  //    b32:b35 - special printing method
  //    b36:b39 - source data to print
  //    b40:b47 - alignment printing
  CFG_ASSERT(file.is_open());
  // Validate before print
  uint64_t total_bit_size = validate();
  file << CFG_print("%s%s\n", space.c_str(), name.c_str()).c_str();
  space += "  ";
  for (auto& r : rules) {
    if (r.size > 0) {
      file << CFG_print("%s%s\n", space.c_str(), r.name.c_str()).c_str();
      uint64_t print_info = get_print_info(r.name);
      if (compress || print_info <= 5) {
        if (compress && print_info > 5) {
          print_info = 1;
        }
        if (compress || print_info != 0 || r.size > 64) {
          // 1 : 1
          // 2 : 2
          // 3 : 4
          // 4 : 8
          // 5 : 16
          uint64_t u8 = convert_to8(r.size);
          uint32_t data_alignment =
              print_info == 0 ? 1 : (1 << (print_info - 1));
          file << space.c_str()
               << CFG_print("  Size (Bit: %d, Byte: %d)\n", r.size, u8).c_str();
          CFG_print_hex(file, &r.data[0], u8, data_alignment, space + "    ",
                        detail);
        } else {
          std::string binary = "";
          uint64_t data = 0;
          for (uint64_t i = 0; i < r.size; i++) {
            if (r.data[i >> 3] & (1 << (i & 7))) {
              binary = "1" + binary;
              data |= (uint64_t(1) << i);
            } else {
              binary = "0" + binary;
            }
          }
          binary = CFG_print("%d\'b%s", r.size, binary.c_str());
          file << CFG_print("%s  %s (0x%lX) (%lu)\n", space.c_str(),
                            binary.c_str(), data, data);
        }
      } else {
        uint32_t print_type = (uint32_t)(print_info >> 32) & 0xF;
        uint32_t source_data_type = (uint32_t)(print_info >> 36) & 0xF;
        uint32_t data_alignment = (uint32_t)(print_info >> 40) & 0xFF;
        uint32_t print_detail = (uint32_t)(print_info);
        // Only support type1 for now
        CFG_ASSERT(print_type == 1 || print_type == 2);
        if (print_type == 1) {
          CFG_ASSERT(print_detail);
          std::vector<uint8_t> source_data;
          std::vector<uint8_t>* data_to_print;
          if (source_data_type == 1) {
            get_source_data_size(source_data, r.name);
            get_source_data(source_data, r.name);
            data_to_print = &source_data;
          } else {
            data_to_print = &r.data;
          }
          CFG_print_binary_line_by_line(
              file, &(*data_to_print)[0], data_to_print->size() * 8,
              (uint64_t)(print_detail), (uint64_t)data_alignment, space + "  ",
              detail);
        } else {
          CFG_ASSERT(print_detail);
          uint64_t u8 = convert_to8(r.size);
          file << space.c_str()
               << CFG_print("  Size (Bit: %d, Byte: %d)\n", r.size, u8).c_str();
          size_t print_size = 0;
          if (data_alignment == 0) {
            data_alignment = 1;
          }
          for (size_t i = 0, j = 0; i < r.data.size(); i += print_size, j++) {
            print_size = r.data.size() - i;
            if (print_size > print_detail) {
              print_size = print_detail;
            }
            file << CFG_print("%s    #%lu (Byte: %lu)\n", space.c_str(), j,
                              print_size)
                        .c_str();
            CFG_print_hex(file, &r.data[i], print_size, data_alignment,
                          space + "      ", detail);
          }
        }
      }
    }
  }
  return total_bit_size;
}

void BitGen_DATA::check_rule_size(BitGen_DATA_RULE* rule, size_t& incomplete,
                                  uint64_t& continuous_size) {
  if (!check_rule_size_readiness(rule)) {
    incomplete++;
    if (rule->size_type == 1) {
      set_sizes(rule->name);
      if (rule->size == 0) {
        continuous_size = uint64_t(-1);
      } else {
        incomplete--;
      }
    } else if (rule->size_type == 2) {
      rule->info |= 1;
      incomplete--;
    } else {
      // This is reserved
      // To auto-determine reserve size, all previous rule must be
      // determined first. Any unknown rule will stop the auto-determination
      // for next search. If we success determine the size, we set MSB of
      // size_type (we do not reply on size because the reserved size can be
      // 0)
      CFG_ASSERT(rule->size_type >= 8 && rule->size_type <= 13);
      if (continuous_size != uint64_t(-1)) {
        // All previous data is complete
        uint64_t alignment = 8 << (rule->size_type - 8);
        uint64_t remaining = continuous_size % alignment;
        if (remaining != 0) {
          CFG_ASSERT(rule->data.size() == 0);
          rule->size = alignment - remaining;
        }
        rule->info |= 1;
        incomplete--;
      }
    }
  }
  if (rule->size != 0 && continuous_size != uint64_t(-1)) {
    continuous_size += rule->size;
  }
}

uint64_t BitGen_DATA::validate() {
  // Make sure size is good
  uint64_t total_bit_size = 0;
  for (auto& rule : rules) {
    if (rule.size != 0) {
      total_bit_size += rule.size;
    }
  }
  CFG_ASSERT(total_bit_size > 0);
  CFG_ASSERT(size_alignment == 0 || (total_bit_size % size_alignment) == 0);

  // Make sure data is not set out of range
  for (auto& rule : rules) {
    if (rule.size > 0) {
      uint64_t align8_size = align8(rule.size);
      CFG_ASSERT(rule.data.size() >= (align8_size / 8));
      for (uint64_t i = rule.size; i < align8_size; i++) {
        CFG_ASSERT((rule.data[i >> 3] & (1 << (i & 7))) == 0);
      }
    }
  }
  auto_defined();
  return total_bit_size;
}

uint64_t BitGen_DATA::convert_to(uint64_t value, uint64_t unit) {
  CFG_ASSERT(unit > 0);
  return ((value + (unit - 1)) / unit);
}

uint64_t BitGen_DATA::convert_to8(uint64_t value) {
  return convert_to(value, 8);
}

uint64_t BitGen_DATA::convert_to16(uint64_t value) {
  return convert_to(value, 16);
}

uint64_t BitGen_DATA::convert_to32(uint64_t value) {
  return convert_to(value, 32);
}

uint64_t BitGen_DATA::convert_to64(uint64_t value) {
  return convert_to(value, 64);
}

uint64_t BitGen_DATA::align(uint64_t value, uint64_t alignment) {
  return convert_to(value, alignment) * alignment;
}

uint64_t BitGen_DATA::align8(uint64_t value) { return align(value, 8); }

uint64_t BitGen_DATA::align16(uint64_t value) { return align(value, 16); }

uint64_t BitGen_DATA::align32(uint64_t value) { return align(value, 32); }

uint64_t BitGen_DATA::align64(uint64_t value) { return align(value, 64); }

bool BitGen_DATA::check_rule_size_readiness(BitGen_DATA_RULE* rule) {
  // For a rule to be ready, the size and data have been determined
  bool ready = false;
  if ((rule->size > 0) || ((rule->info & 1) != 0)) {
    ready = true;
  }
  return ready;
}

bool BitGen_DATA::check_rule_size_readiness(const uint32_t field_enum) {
  // Check if the size is ready
  return check_rule_size_readiness(get_rule(field_enum));
}

bool BitGen_DATA::check_rules_readiness(std::vector<uint32_t> field_enums) {
  // For a rule to be ready, the size and data have been determined
  bool ready = true;
  for (auto field_enum : field_enums) {
    if (check_rule_size_readiness(field_enum)) {
      BitGen_DATA_RULE* rule = get_rule(field_enum);
      if (rule->data.size() == 0) {
        ready = false;
        break;
      }
    } else {
      ready = false;
      break;
    }
  }
  return ready;
}

bool BitGen_DATA::check_all_rules_readiness_but(
    std::vector<uint32_t> field_enums) {
  // For a rule to be ready, the size and data have been determined
  bool ready = true;
  for (uint32_t i = 0; i < uint32_t(rules.size()); i++) {
    if (CFG_find_u32_in_vector(field_enums, i) >= 0) {
      if (check_rule_size_readiness(i)) {
        BitGen_DATA_RULE* rule = get_rule(i);
        if (rule->data.size() == 0) {
          ready = false;
          break;
        }
      } else {
        ready = false;
        break;
      }
    }
  }
  return ready;
}

BitGen_DATA_RULE* BitGen_DATA::get_rule(const std::string& field) {
  BitGen_DATA_RULE* rule = nullptr;
  for (auto& r : rules) {
    if (r.name == field) {
      rule = &r;
    }
  }
  return rule;
}

BitGen_DATA_RULE* BitGen_DATA::get_rule(const uint32_t field_enum) {
  CFG_ASSERT(field_enum < rules.size());
  BitGen_DATA_RULE* rule = &rules[field_enum];
  CFG_ASSERT(rule != nullptr);
  return rule;
}

BitGEN_SRC_DATA* BitGen_DATA::get_src(const std::string& name) {
  auto src_iter = srcs.find(name);
  CFG_ASSERT(src_iter != srcs.end());
  return &src_iter->second;
}

uint64_t BitGen_DATA::get_rule_size(const std::string& field) {
  BitGen_DATA_RULE* rule = get_rule(field);
  CFG_ASSERT(rule != nullptr);
  return rule->size;
}

uint64_t BitGen_DATA::get_rule_value(const std::string& field) {
  uint64_t value = 0;
  BitGen_DATA_RULE* rule = get_rule(field);
  CFG_ASSERT(rule != nullptr);
  CFG_ASSERT(rule->size > 0 && rule->size <= 64);
  size_t length = (size_t)(convert_to8(rule->size));
  CFG_ASSERT(length == rule->data.size());
  memcpy(reinterpret_cast<uint8_t*>(&value), &rule->data[0], length);
  return value;
}

std::vector<uint8_t> BitGen_DATA::get_rule_non_reference_data(
    const std::string& field) {
  BitGen_DATA_RULE* rule = get_rule(field);
  CFG_ASSERT(rule != nullptr);
  return rule->data;
}

std::vector<uint8_t>& BitGen_DATA::get_rule_data(const std::string& field) {
  BitGen_DATA_RULE* rule = get_rule(field);
  CFG_ASSERT(rule != nullptr);
  return rule->data;
}

std::vector<uint8_t>* BitGen_DATA::get_rule_data_ptr(const std::string& field) {
  BitGen_DATA_RULE* rule = get_rule(field);
  CFG_ASSERT(rule != nullptr);
  return &(rule->data);
}

uint64_t BitGen_DATA::get_src_value(const std::string& name) {
  uint64_t value = 0;
  BitGEN_SRC_DATA* src = get_src(name);
  CFG_ASSERT(src->data.size() == sizeof(value));
  memcpy(&value, &src->data[0], sizeof(value));
  return value;
}

std::vector<uint8_t>& BitGen_DATA::get_src_data(const std::string& name) {
  BitGEN_SRC_DATA* src = get_src(name);
  return src->data;
}

uint64_t BitGen_DATA::get_src_data_size(const std::string& name) {
  BitGEN_SRC_DATA* src = get_src(name);
  return (uint64_t)(src->data.size());
}

uint64_t BitGen_DATA::get_defined_value(const std::string& name) {
  auto defined_iter = defineds.find(name);
  CFG_ASSERT(defined_iter != defineds.end());
  BitGEN_SRC_DATA* defined_data = &defined_iter->second;
  uint64_t value = 0;
  memcpy(&value, &defined_data->data[0], sizeof(value));
  return value;
}

void BitGen_DATA::set_rule_size(const std::string& field, uint64_t size) {
  BitGen_DATA_RULE* rule = get_rule(field);
  rule->size = size;
}

void BitGen_DATA::set_size(BitGen_DATA_RULE* rule, uint64_t value,
                           const uint32_t property) {
  if (!ignore_set_size_property && property != 0) {
    CFG_ASSERT((value % property) == 0);
  }
  rule->size = value;
  rule->info |= 1;
}

void BitGen_DATA::set_size(const uint32_t field_enum, uint64_t value,
                           const uint32_t property) {
  set_size(get_rule(field_enum), value, property);
}

void BitGen_DATA::set_data(BitGen_DATA_RULE* rule, uint64_t value,
                           const uint32_t property) {
  CFG_ASSERT(rule->size > 0 && rule->size <= 64);
  if (property != 0) {
    CFG_ASSERT((value % property) == 0);
  }
  size_t length = (size_t)(convert_to8(rule->size));
  if (rule->data.size() == 0) {
    rule->data.resize(length);
  }
  CFG_ASSERT(length == rule->data.size());
  memcpy(&rule->data[0], reinterpret_cast<uint8_t*>(&value), length);
}

void BitGen_DATA::set_data(const uint32_t field_enum, uint64_t value,
                           const uint32_t property) {
  set_data(get_rule(field_enum), value, property);
}

void BitGen_DATA::set_data(BitGen_DATA_RULE* rule, std::vector<uint8_t> data) {
  CFG_ASSERT_MSG(rule->size > 0, "%s set_data() failed", rule->name.c_str());
  size_t length = (size_t)(convert_to8(rule->size));
  if (rule->data.size() == 0) {
    rule->data.resize(length);
  }
  CFG_ASSERT(length == rule->data.size());
  CFG_ASSERT(length <= data.size())
  memcpy(&rule->data[0], &data[0], length);
}

void BitGen_DATA::set_data(const uint32_t field_enum,
                           std::vector<uint8_t> data) {
  set_data(get_rule(field_enum), data);
}

std::vector<uint8_t> BitGen_DATA::genbits_line_by_line(
    std::vector<uint8_t>& src_data, uint64_t line_bits, uint64_t total_line,
    uint64_t src_unit_bits, uint64_t dest_unit_bits, bool pad_reversed,
    bool unit_reversed, const std::vector<uint8_t>* dest_data_ptr) {
  std::vector<uint8_t> dest_data;
  CFG_ASSERT(line_bits > 0);
  CFG_ASSERT(total_line > 0);
  CFG_ASSERT(src_unit_bits > 0);
  CFG_ASSERT(dest_unit_bits > 0);
  uint64_t src_line_aligned_bits = align(line_bits, src_unit_bits);
  uint64_t dest_line_aligned_bits = align(line_bits, dest_unit_bits);
  CFG_ASSERT((src_line_aligned_bits * total_line) == (src_data.size() * 8));
  uint64_t padding_bits = dest_line_aligned_bits - line_bits;
  CFG_ASSERT(padding_bits < dest_unit_bits);
  dest_data.resize(convert_to8(dest_line_aligned_bits * total_line));
  bool reversing = false;
  if (dest_data_ptr != nullptr) {
    CFG_ASSERT(dest_data.size() == dest_data_ptr->size());
    memcpy(&dest_data[0], &(*dest_data_ptr)[0], dest_data.size());
    reversing = true;
  } else {
    memset(&dest_data[0], 0, dest_data.size());
  }
  for (uint64_t line = 0; line < total_line; line++) {
    size_t src_index = size_t(line * src_line_aligned_bits);
    size_t dest_index = size_t(line * dest_line_aligned_bits);
    size_t original_dest_index = dest_index;
    size_t end_dest_index = original_dest_index + dest_line_aligned_bits;
    if (unit_reversed) {
      dest_index += (dest_line_aligned_bits - dest_unit_bits);
    }
    if (pad_reversed) {
      dest_index += padding_bits;
    }
    for (uint64_t bit = 0; bit < line_bits; bit++) {
      if (reversing) {
        if (dest_data[dest_index >> 3] & (1 << (dest_index & 7))) {
          src_data[src_index >> 3] |= (1 << (src_index & 7));
        }
      } else {
        if (src_data[src_index >> 3] & (1 << (src_index & 7))) {
          dest_data[dest_index >> 3] |= (1 << (dest_index & 7));
        }
      }
      src_index++;
      dest_index++;
      if (unit_reversed && bit < (line_bits - 1) &&
          (dest_index % dest_unit_bits) == 0) {
        dest_index -= (2 * dest_unit_bits);
        CFG_ASSERT(dest_index >= original_dest_index);
        CFG_ASSERT(dest_index < end_dest_index);
      }
    }
  }
  return dest_data;
}

uint64_t BitGen_DATA::calc_checksum(std::vector<uint8_t>& data, uint8_t type) {
  uint64_t checksum = 0;
  if (type == 0) {
    // Gemini FCB (Flecther-32)
    CFG_ASSERT(data.size() > 0 && (data.size() % 4) == 0);
    uint16_t* data_ptr = reinterpret_cast<uint16_t*>(&data[0]);
    size_t u16_length = data.size() / 2;
    uint32_t c0 = 0;
    uint32_t c1 = 0;
    uint32_t type0_checksum = 0;
    for (size_t i = 0; i < u16_length;) {
      c0 += data_ptr[i++];
      c1 += c0;
    }
    type0_checksum = (c1 << 16) | ((-(c1 + c0)) & 0x0000ffff);
    checksum = (uint64_t)(type0_checksum);
  } else {
    CFG_INTERNAL_ERROR("Does not support checksum type %d", type);
  }
  return checksum;
}

uint64_t BitGen_DATA::calc_crc(const std::string& name, uint8_t type,
                               uint8_t data_type) {
  // type
  //   0 : crc16 A001
  // data_type
  //   0 : all the rule before me
  uint64_t crc = 0;
  std::vector<uint8_t> data;
  if (data_type == 0) {
    uint64_t dest = 0;
    for (auto& rule : rules) {
      if (rule.name != name && rule.size != 0) {
        for (uint64_t i = 0; i < rule.size; i++, dest++) {
          if ((dest % 8) == 0) {
            data.push_back(0);
          }
          if (rule.data[i >> 3] & (1 << (i & 7))) {
            data[dest >> 3] |= (1 << (dest & 7));
          }
        }
      }
    }
  } else {
    CFG_INTERNAL_ERROR("Does not support CRC data type %d", data_type);
  }
  if (type == 0) {
    // crc16 A001
    crc = (uint64_t)(CFG_bop_A001_crc16(&data[0], data.size()) & 0xFFFF);
  } else {
    CFG_INTERNAL_ERROR("Does not support CRC type %d", type);
  }
  return crc;
}

uint8_t BitGen_DATA::set_sizes(const std::string& field) {
  CFG_INTERNAL_ERROR("Unimplemented virtual function");
  return 0;
}

uint8_t BitGen_DATA::set_datas(const std::string& field) {
  CFG_INTERNAL_ERROR("Unimplemented virtual function");
  return 0;
}

void BitGen_DATA::auto_defined() {
  CFG_INTERNAL_ERROR("Unimplemented virtual function");
}

void BitGen_DATA::get_source_data_size(std::vector<uint8_t>& data,
                                       const std::string& field) {
  CFG_INTERNAL_ERROR("Unimplemented virtual function");
}

void BitGen_DATA::get_source_data(std::vector<uint8_t>& data,
                                  const std::string& field) {
  CFG_INTERNAL_ERROR("Unimplemented virtual function");
}

uint64_t BitGen_DATA::get_print_info(const std::string& field) {
  CFG_INTERNAL_ERROR("Unimplemented virtual function");
  return 0;
}

#include "BitGen_data_auto.cpp"
