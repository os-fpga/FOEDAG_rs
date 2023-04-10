#ifndef CFGOBJECT_H
#define CFGOBJECT_H

#include <algorithm>
#include <string>
#include <vector>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"

// Never change the ordering of the supported types
// Otherwise you will mess up the enum in the out-going file
const std::vector<std::string> SUPPORTED_TYPES = {
    "bool", "u8",   "u16",  "u32",  "u64",  "i32", "i64",   "u8s",
    "u16s", "u32s", "u64s", "i32s", "i64s", "str", "class", "list"};

struct CFGObject_RULE {
  CFGObject_RULE(std::string n, bool e, bool c, void* p, const std::string& t)
      : name(n), exist(e), compress(c), ptr(p), type(t) {
    // name
    CFG_ASSERT(name.size() >= 2 && name.size() <= 16);
    // ptr  must not be nullptr
    CFG_ASSERT(ptr != nullptr);
    // type must be supported
    auto iter = std::find(SUPPORTED_TYPES.begin(), SUPPORTED_TYPES.end(), type);
    CFG_ASSERT(iter != SUPPORTED_TYPES.end());
  }
  void set_exist(bool e) const {
    bool* is_exist_ptr = const_cast<bool*>(&is_exist);
    *is_exist_ptr = e;
  }
  const std::string name;
  const bool exist;
  const bool compress;
  const void* ptr;
  const std::string type;
  bool is_exist = false;
};

class CFGObject {
 public:
  CFGObject() {}
  CFGObject(const std::string& n, const std::vector<CFGObject_RULE>& r)
      : name(n), rules(r) {}

  // Write individual member
  void write_bool(const std::string& name, bool value) const;
  void write_u8(const std::string& name, uint8_t value) const;
  void write_u16(const std::string& name, uint16_t value) const;
  void write_u32(const std::string& name, uint32_t value) const;
  void write_u64(const std::string& name, uint64_t value) const;
  void write_i32(const std::string& name, int32_t value) const;
  void write_i64(const std::string& name, int64_t value) const;
  void write_u8s(const std::string& name, std::vector<uint8_t> value) const;
  void write_u16s(const std::string& name, std::vector<uint16_t> value) const;
  void write_u32s(const std::string& name, std::vector<uint32_t> value) const;
  void write_u64s(const std::string& name, std::vector<uint64_t> value) const;
  void write_i32s(const std::string& name, std::vector<int32_t> value) const;
  void write_i64s(const std::string& name, std::vector<int64_t> value) const;
  void write_str(const std::string& name, const std::string& value) const;
  void append_u8s(const std::string& name, std::vector<uint8_t> value) const;
  void append_u16s(const std::string& name, std::vector<uint16_t> value) const;
  void append_u32s(const std::string& name, std::vector<uint32_t> value) const;
  void append_u64s(const std::string& name, std::vector<uint64_t> value) const;
  void append_i32s(const std::string& name, std::vector<int32_t> value) const;
  void append_i64s(const std::string& name, std::vector<int64_t> value) const;
  void append_str(const std::string& name, const std::string& value) const;
  void append_u8(const std::string& name, uint8_t value) const;
  void append_u16(const std::string& name, uint16_t value) const;
  void append_u32(const std::string& name, uint32_t value) const;
  void append_u64(const std::string& name, uint64_t value) const;
  void append_i32(const std::string& name, int32_t value) const;
  void append_i64(const std::string& name, int64_t value) const;
  void append_char(const std::string& name, char value) const;

  // File IO
  bool write(const std::string& filepath,
             std::vector<std::string>* errors = nullptr);
  bool read(const std::string& filepath,
            std::vector<std::string>* errors = nullptr);

  // Generic, Helper (Public)
  void set_parent_ptr(const CFGObject* pp) const;
  uint64_t get_object_count() const;
  bool check_exist(const std::string& name) const;
  // Static
  static void parse(const std::string& input_filepath,
                    const std::string& output_filepath, bool detail);
  static std::string get_string(const uint8_t* data, size_t data_size,
                                size_t& index, int max_size = -1,
                                int min_size = -1, int null_check = -1);
  template <typename T>
  static void deserialize_data(const uint8_t* data, size_t data_size,
                               size_t& index, T& value);
  template <typename T>
  static std::vector<T> read_raw_datas(const uint8_t* data, size_t data_size,
                                       size_t& index, T value);

 protected:
  // Generic, Helper
  const CFGObject_RULE* get_rule(const std::string& name) const;
  const CFGObject_RULE* get_rule(const void* ptr) const;
  void update_exist(const CFGObject_RULE* rule) const;
  void update_exist(const std::string& name) const;
  bool check_rule(std::vector<std::string>* errors) const;
  bool check_exist(std::vector<std::string>* errors) const;
  void post_error(const std::string& msg,
                  std::vector<std::string>* errors) const;
  uint8_t get_type_enum(const std::string& type) const;

  // Write
  void serialize(std::vector<uint8_t>& data) const;
  void serialize_type_and_name(std::vector<uint8_t>& data,
                               const CFGObject_RULE* rule) const;
  void serialize_value(std::vector<uint8_t>& data,
                       const CFGObject_RULE* rule) const;

  // Read
  void parse_object(const uint8_t* data, size_t data_size, size_t& index,
                    size_t& object_count) const;
  void parse_class_object(const uint8_t* data, size_t data_size, size_t& index,
                          size_t& object_count) const;

  // Using template
  template <typename T>
  void write_data(const CFGObject_RULE* rule, T value) const;
  template <typename T>
  void write_data(const std::string& name, const std::string& type,
                  T value) const;
  template <typename T>
  void read_data(const uint8_t* data, size_t data_size, size_t& index,
                 const CFGObject_RULE* rule, T value) const;
  template <typename T>
  void read_datas(const uint8_t* data, size_t data_size, size_t& index,
                  const CFGObject_RULE* rule, T value) const;
  template <typename T>
  void append_datas(const std::string& name, const std::string& type,
                    T value) const;
  template <typename T>
  void append_data(const std::string& name, const std::string& type,
                   T value) const;
  template <typename T>
  void serialize_data(std::vector<uint8_t>& data, T value) const;

  template <typename T>
  void serialize_datas(std::vector<uint8_t>& data, std::vector<T>& value,
                       bool compress) const;

  // Members
  const CFGObject* parent_ptr = nullptr;
  const std::string name = "";
  const std::vector<CFGObject_RULE> rules;
};

#endif
