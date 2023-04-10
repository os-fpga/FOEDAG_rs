#ifndef BITGEN_DATA_H
#define BITGEN_DATA_H

#include <map>
#include <string>
#include <vector>

#include "CFGCommonRS/CFGCommonRS.h"

struct BitGen_DATA_RULE {
  BitGen_DATA_RULE(const std::string& n, uint64_t s, uint64_t d, uint8_t t,
                   uint32_t p)
      : name(n), size(s), default_value(d), size_type(t), property(p) {
    CFG_ASSERT(name.size());
  }
  ~BitGen_DATA_RULE() {
    if (data.size()) {
      memset(&data[0], 0, data.size());
      data.resize(0);
    }
  }
  const std::string name = "";
  uint64_t size = 0;
  const uint64_t default_value = 0;
  const uint8_t size_type = 0;
  uint8_t info = 0;
  std::vector<uint8_t> data = {};
  const uint32_t property = 0;
};

struct BitGEN_SRC_DATA {
  BitGEN_SRC_DATA() {}
  BitGEN_SRC_DATA(uint64_t value) {
    data.resize(sizeof(value));
    memcpy(&data[0], &value, sizeof(value));
  }
  BitGEN_SRC_DATA(const std::vector<uint8_t>& value) {
    data.resize(value.size());
    memcpy(&data[0], &value[0], value.size());
  }
  ~BitGEN_SRC_DATA() {
    if (data.size()) {
      memset(&data[0], 0, data.size());
      data.resize(0);
    }
  }
  std::vector<uint8_t> data = {};
};

class BitGen_DATA {
 public:
  BitGen_DATA(const std::string& n, const uint8_t s,
              std::vector<BitGen_DATA_RULE> r);
  void set_rule_size(const std::string& field, uint64_t size);
  void set_src(const std::string& name, uint64_t value);
  void set_src(const std::string& name, const std::vector<uint8_t>& data);
  void set_src(const std::string& name, const uint8_t* data, size_t data_size);
  uint64_t generate(std::vector<uint8_t>& data, std::vector<uint8_t>& key,
                    uint8_t* iv, std::vector<std::string> data_to_encrypt = {});
  uint64_t parse(std::ofstream& file, const uint8_t* data,
                 uint64_t total_bit_size, uint64_t& bit_index,
                 std::string space, const bool detail = false);
  BitGen_DATA_RULE* get_rule(const std::string& field);
  uint64_t get_rule_size(const std::string& field);
  uint64_t get_rule_value(const std::string& field);
  std::vector<uint8_t>& get_rule_data(const std::string& field);
  std::vector<uint8_t>* get_rule_data_ptr(const std::string& field);
  uint64_t get_header_size();

 protected:
  uint64_t validate();
  uint64_t print(std::ofstream& file, std::string space, const bool detail);
  uint64_t convert_to(uint64_t value, uint64_t unit);
  uint64_t convert_to8(uint64_t value);
  uint64_t convert_to16(uint64_t value);
  uint64_t convert_to32(uint64_t value);
  uint64_t convert_to64(uint64_t value);
  uint64_t align(uint64_t value, uint64_t alignment);
  uint64_t align8(uint64_t value);
  uint64_t align16(uint64_t value);
  uint64_t align32(uint64_t value);
  uint64_t align64(uint64_t value);
  void check_rule_size(BitGen_DATA_RULE* rule, size_t& incomplete,
                       uint64_t& continuous_size);
  bool check_rule_size_readiness(BitGen_DATA_RULE* rule);
  bool check_rule_size_readiness(const uint32_t field_enum);
  bool check_rules_readiness(std::vector<uint32_t> field_enums);
  bool check_all_rules_readiness_but(std::vector<uint32_t> field_enums);
  BitGen_DATA_RULE* get_rule(const uint32_t field_enum);
  BitGEN_SRC_DATA* get_src(const std::string& name);
  uint64_t get_src_value(const std::string& name);
  std::vector<uint8_t>& get_src_data(const std::string& name);
  uint64_t get_src_data_size(const std::string& name);
  uint64_t get_defined_value(const std::string& name);
  void set_size(BitGen_DATA_RULE* rule, uint64_t value,
                const uint32_t property = 0);
  void set_size(const uint32_t field_enum, uint64_t value,
                const uint32_t property = 0);
  void set_data(BitGen_DATA_RULE* rule, uint64_t value,
                const uint32_t property = 0);
  void set_data(const uint32_t field_enum, uint64_t value,
                const uint32_t property = 0);
  void set_data(BitGen_DATA_RULE* rule, std::vector<uint8_t> data);
  void set_data(const uint32_t field_enum, std::vector<uint8_t> data);
  std::vector<uint8_t> genbits_line_by_line(
      std::vector<uint8_t>& src_data, uint64_t line_bits, uint64_t total_line,
      uint64_t src_unit_bits, uint64_t dest_unit_bits, bool pad_reversed,
      bool unit_reversed, const std::vector<uint8_t>* dest_data_ptr = nullptr);
  uint64_t calc_checksum(std::vector<uint8_t>& data, uint8_t type);
  uint64_t calc_crc(const std::string& name, uint8_t type, uint8_t data_type);

  // Virtual function
  virtual uint8_t set_sizes(const std::string& field);
  virtual uint8_t set_datas(const std::string& field);
  virtual void auto_defined();
  virtual void get_source_data_size(std::vector<uint8_t>& data,
                                    const std::string& field);
  virtual void get_source_data(std::vector<uint8_t>& data,
                               const std::string& field);
  virtual uint64_t get_print_info(const std::string& field);

  std::map<std::string, BitGEN_SRC_DATA> srcs = {};
  std::map<std::string, BitGEN_SRC_DATA> defineds = {};

 private:
  const std::string name = "";
  const uint8_t size_alignment = 0;
  std::vector<BitGen_DATA_RULE> rules = {};
};

#endif