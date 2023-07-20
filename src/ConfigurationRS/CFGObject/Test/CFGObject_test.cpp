#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGObject_auto.h"
#include "nlohmann_json/json.hpp"

const std::vector<std::string> EMPTY_OBJECT_ERRORS = {
    "boolean does not exist",       "u8 does not exist",
    "u16 does not exist",           "u32 does not exist",
    "i64 does not exist",           "u8s does not exist",
    "i64s does not exist",          "str does not exist",
    "strs does not exist",          "list0 does not exist",
    "object does not exist",        "cmp does not exist",
    "data_after_cmp does not exist"};

const std::vector<std::string> FIRST_TEST_OBJECT_ERRORS = {
    "u8 does not exist",    "u32 does not exist",  "i64s does not exist",
    "str does not exist",   "i32s does not exist", "list01 does not exist",
    "object does not exist"};

const std::vector<std::string> SECOND_TEST_OBJECT_ERRORS = {
    "u64s does not exist", "str0 does not exist"};

int main(int argc, const char** argv) {
  CFG_POST_MSG("This is CFGObject unit test");
  CFGObject_UTST utst;
  std::vector<std::string> errors;
  CFG_ASSERT(!utst.write("utst.bin", &errors));
  CFG_ASSERT_MSG(errors.size() == EMPTY_OBJECT_ERRORS.size(),
                 "Expect %ld error(s) but found %ld",
                 EMPTY_OBJECT_ERRORS.size(), errors.size());
  size_t index = 0;
  for (auto str : errors) {
    CFG_ASSERT_MSG(str == EMPTY_OBJECT_ERRORS[index],
                   "Empty Object Test: \"%s\" vs \"%s\"", str.c_str(),
                   EMPTY_OBJECT_ERRORS[index].c_str());
    index++;
  }
  errors.clear();

  // Write some data and re-check
  CFG_ASSERT(!utst.boolean);
  utst.write_bool("boolean", true);
  CFG_ASSERT(utst.boolean);

  CFG_ASSERT(utst.u16 == 0);
  utst.write_u16("u16", 0x16);
  CFG_ASSERT(utst.u16 == 0x16);

  CFG_ASSERT(utst.i64 == 0);
  utst.write_i64("i64", -1);
  CFG_ASSERT(utst.i64 == -1);

  CFG_ASSERT(utst.u8s.size() == 0);
  utst.write_u8s("u8s", {0, 1, 2, 3, 4});
  utst.append_u8s("u8s", {5, 6, 7, 8, 9});
  utst.append_u8("u8s", 10);

  std::vector<uint8_t> cmp_test_data = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4,
                                        0, 1, 2, 3, 4, 0, 1, 2, 3, 4,
                                        0, 0, 0, 0, 0, 0, 0, 0};
  utst.append_u8s("cmp", cmp_test_data);
  utst.write_u64("data_after_cmp", 0x1234567890ABCDEF);
  CFG_ASSERT(utst.u8s.size() == 11);
  for (size_t i = 0; i < utst.u8s.size(); i++) {
    CFG_ASSERT(i == utst.u8s[i]);
  }

  utst.create_child("list0");
  CFG_ASSERT(utst.list0.back()->u64 == 0);
  utst.list0.back()->write_u64("u64", 1);
  CFG_ASSERT(utst.list0.back()->u64 == 1);

  CFG_ASSERT(utst.list0.back()->u16s.size() == 0);
  utst.list0.back()->append_u16("u16s", 1000);
  CFG_ASSERT(utst.list0.back()->u16s.size() == 1);
  CFG_ASSERT(utst.list0.back()->u16s[0] == 1000);

  utst.write_strs("strs", {"abc", "def"});
  utst.append_strs("strs", {"ghi", "", "jkl"});

  // Check the error msg again
  CFG_ASSERT(!utst.write("utst.bin", &errors));
  CFG_ASSERT(errors.size() == FIRST_TEST_OBJECT_ERRORS.size());
  index = 0;
  for (auto str : errors) {
    CFG_ASSERT_MSG(str == FIRST_TEST_OBJECT_ERRORS[index],
                   "First test Object Test: \"%s\" vs \"%s\"", str.c_str(),
                   FIRST_TEST_OBJECT_ERRORS[index].c_str());
    index++;
  }
  errors.clear();

  // Assign almost all needed data
  utst.write_u8("u8", 3);
  utst.write_u32("u32", 4);
  utst.write_i64s("i64s", {4, 3, -2, -1});
  utst.write_str("str", "this is unit test");
  utst.append_char("str", '!');
  CFG_ASSERT(utst.str == "this is unit test!");
  utst.list0.back()->write_i32s("i32s", {-10, -9, -8});
  utst.list0.back()->create_child("list01");
  utst.object.write_u8s("u8s0", {8, 9});

  // Check the error msg again
  CFG_ASSERT(!utst.write("utst.bin", &errors));
  CFG_ASSERT(errors.size() == SECOND_TEST_OBJECT_ERRORS.size());
  index = 0;
  for (auto str : errors) {
    CFG_ASSERT_MSG(str == SECOND_TEST_OBJECT_ERRORS[index],
                   "Second test Object Test: \"%s\" vs \"%s\"", str.c_str(),
                   SECOND_TEST_OBJECT_ERRORS[index].c_str());
    index++;
  }
  errors.clear();

  // Assign all needed data
  utst.list0.back()->list01.back()->write_u64s("u64s", {0, 1, 2});
  utst.object.write_str("str0", "This is second string");

  // Successfully write
  CFG_ASSERT(utst.write("utst.bin", &errors));
  CFG_ASSERT(errors.size() == 0);

  // Read back
  CFGObject_UTST rdback;
  CFG_ASSERT(rdback.read("utst.bin", &errors));
  CFG_ASSERT(errors.size() == 0);
  CFG_ASSERT(rdback.cmp.size() == cmp_test_data.size());
  for (size_t i = 0; i < cmp_test_data.size(); i++) {
    CFG_ASSERT(rdback.cmp[i] == cmp_test_data[i]);
  }
  CFG_ASSERT(rdback.strs.size() == 5);
  CFG_ASSERT(rdback.strs[0] == "abc");
  CFG_ASSERT(rdback.strs[1] == "def");
  CFG_ASSERT(rdback.strs[2] == "ghi");
  CFG_ASSERT(rdback.strs[3] == "");
  CFG_ASSERT(rdback.strs[4] == "jkl");
  CFG_ASSERT(rdback.data_after_cmp == 0x1234567890ABCDEF);
  return 0;
}
