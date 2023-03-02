/*
Copyright 2021 The Foedag team

GPL License

Copyright (c) 2021 The Open-Source FPGA Foundation

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

#include <stdio.h>

#include "CFGCommon/CFGHelper.h"
#include "CFGObject_auto.h"

const std::vector<std::string> EMPTY_OBJECT_ERRORS = {
  "boolean does not exist",
  "u8 does not exist",
  "u16 does not exist",
  "u32 does not exist",
  "i64 does not exist",
  "u8s does not exist",
  "i64s does not exist",
  "str does not exist",
  "list0 does not exist",
  "object does not exist"
};

const std::vector<std::string> FIRST_TEST_OBJECT_ERRORS = {
  "u8 does not exist",
  "u32 does not exist",
  "i64s does not exist",
  "str does not exist",
  "i32s does not exist",
  "list01 does not exist",
  "object does not exist"
};

const std::vector<std::string> SECOND_TEST_OBJECT_ERRORS = {
  "u64s does not exist",
  "str0 does not exist"
}; 

int main(int argc, char** argv) {
  printf("This is CFGObject unit test\n");
  CFGObject_UTST utst;
  CFG_ASSERT(!utst.write("utst.bin"));
  CFG_ASSERT(utst.error_msgs.size() == EMPTY_OBJECT_ERRORS.size());
  size_t index = 0;
  for (auto str : utst.error_msgs) {
    CFG_ASSERT_MSG(str == EMPTY_OBJECT_ERRORS[index], 
                    "Empty Object Test: \"%s\" vs \"%s\"", str.c_str(), EMPTY_OBJECT_ERRORS[index].c_str());
    index++;
  }

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

  // Check the error msg again
  CFG_ASSERT(!utst.write("utst.bin"));
  CFG_ASSERT(utst.error_msgs.size() == FIRST_TEST_OBJECT_ERRORS.size());
  index = 0;
  for (auto str : utst.error_msgs) {
    CFG_ASSERT_MSG(str == FIRST_TEST_OBJECT_ERRORS[index], 
                    "First test Object Test: \"%s\" vs \"%s\"", str.c_str(), FIRST_TEST_OBJECT_ERRORS[index].c_str());
    index++;
  }

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
  CFG_ASSERT(!utst.write("utst.bin"));
  CFG_ASSERT(utst.error_msgs.size() == SECOND_TEST_OBJECT_ERRORS.size());
  index = 0;
  for (auto str : utst.error_msgs) {
    CFG_ASSERT_MSG(str == SECOND_TEST_OBJECT_ERRORS[index], 
                    "Second test Object Test: \"%s\" vs \"%s\"", str.c_str(), SECOND_TEST_OBJECT_ERRORS[index].c_str());
    index++;
  }

  // Assign all needed data
  utst.list0.back()->list01.back()->write_u64s("u64s", {0, 1, 2});
  utst.object.write_str("str0", "This is second string");

  // Successfully write
  CFG_ASSERT(utst.write("utst.bin"));
  CFG_ASSERT(utst.error_msgs.size() == 0);

  // Read back
  CFGObject_UTST rdback;
  CFG_ASSERT(rdback.read("utst.bin"));
  CFG_ASSERT(rdback.error_msgs.size() == 0);
}
