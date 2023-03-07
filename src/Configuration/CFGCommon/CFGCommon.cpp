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

#include <stdio.h>

#include <fstream>
#include <iostream>

#include "CFGCompress.h"
#include "CFGHelper.h"
#include "CFGMessager.h"

/*
  All about Helper
*/

std::string CFG_print(const char* format_string, ...) {
  char* buf = nullptr;
  size_t bufsize = CFG_PRINT_MINIMUM_SIZE;
  std::string string = "";
  va_list args;
  while (1) {
    buf = new char[bufsize + 1]();
    memset(buf, 0, bufsize + 1);
    va_start(args, format_string);
    size_t n = std::vsnprintf(buf, bufsize + 1, format_string, args);
    va_end(args);
    if (n <= bufsize) {
      string.resize(n);
      memcpy((char*)(&string[0]), buf, n);
      break;
    }
    delete buf;
    buf = nullptr;
    bufsize *= 2;
    if (bufsize > CFG_PRINT_MAXIMUM_SIZE) {
      break;
    }
  }
  if (buf != nullptr) {
    delete buf;
  }
  return string;
}

void CFG_assertion(const char* file, size_t line, std::string msg) {
  printf("Assertion at %s (line: %d)\n", file, (uint32_t)(line));
  printf("   MSG: %s\n\n", msg.c_str());
  exit(-1);
}

std::string CFG_get_time() {
  time_t rawtime;
  struct tm* timeinfo;
  char buffer[80];
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof(buffer), "%d %b %Y %H:%M:%S", timeinfo);
  std::string str(buffer);
  return str;
}

void CFG_get_rid_trailing_whitespace(std::string& string,
                                     const std::vector<char> whitespaces) {
  CFG_ASSERT(whitespaces.size());
  while (string.size()) {
    auto iter =
        std::find(whitespaces.begin(), whitespaces.end(), string.back());
    if (iter != whitespaces.end()) {
      string.pop_back();
    } else {
      break;
    }
  }
}

void CFG_get_rid_leading_whitespace(std::string& string,
                                    const std::vector<char> whitespaces) {
  CFG_ASSERT(whitespaces.size());
  while (string.size()) {
    auto iter =
        std::find(whitespaces.begin(), whitespaces.end(), string.front());
    if (iter != whitespaces.end()) {
      string.erase(0, 1);
    } else {
      break;
    }
  }
}

void CFG_get_rid_whitespace(std::string& string,
                            const std::vector<char> whitespaces) {
  CFG_get_rid_trailing_whitespace(string, whitespaces);
  CFG_get_rid_leading_whitespace(string, whitespaces);
}

CFG_TIME CFG_time_begin() { return std::chrono::high_resolution_clock::now(); }

uint64_t CFG_nano_time_elapse(CFG_TIME begin) {
  CFG_TIME end = std::chrono::high_resolution_clock::now();
  std::chrono::nanoseconds elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
  return (uint64_t)(elapsed.count());
}

float CFG_time_elapse(CFG_TIME begin) {
  return float(CFG_nano_time_elapse(begin) * 1e-9);
}

uint8_t CFG_write_variable_u64(std::vector<uint8_t>& data, uint64_t value) {
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

uint64_t CFG_read_variable_u64(const uint8_t* data, size_t data_size,
                               size_t& index, int max_size) {
  CFG_ASSERT(data != nullptr && data_size > 0);
  uint64_t u64 = 0;
  int current_index = 0;
  while (1) {
    // maximum is uint64_t (10 bytes which including next bit)
    CFG_ASSERT(current_index < 10);
    CFG_ASSERT(index < data_size);
    u64 |= (uint64_t)((uint64_t)(data[index] & 0x7F) << (current_index * 7));
    current_index++;
    if ((data[index] & 0x80) == 0) {
      index++;
      break;
    }
    index++;
  }
  CFG_ASSERT(max_size == -1 || current_index <= max_size);
  return u64;
}

void CFG_read_binary_file(const std::string& filepath,
                          std::vector<uint8_t>& data) {
  // File size to prepare memory
  std::ifstream file(filepath.c_str(), std::ios::binary | std::ios::ate);
  CFG_ASSERT(file.is_open());
  size_t filesize = file.tellg();
  file.close();

  // Read the binary
  CFG_ASSERT(filesize > 0);
  data.clear();
  data.resize(filesize);
  file = std::ifstream(filepath.c_str(), std::ios::in | std::ios::binary);
  CFG_ASSERT(file.is_open());
  file.read((char*)(&data[0]), data.size());
  file.close();
}

void CFG_write_binary_file(const std::string& filepath, const uint8_t* data,
                           const size_t data_size) {
  std::ofstream file(filepath.c_str(), std::ios::out | std::ios::binary);
  CFG_ASSERT(file.is_open());
  file.write((char*)(const_cast<uint8_t*>(data)), data_size);
  file.flush();
  file.close();
}

void CFG_compress(const uint8_t* input, const size_t input_size,
                  std::vector<uint8_t>& output, size_t* header_size,
                  const bool debug, const bool retry) {
  CFG_ASSERT(input != nullptr);
  CFG_ASSERT(input_size > 0);

  std::vector<uint8_t> cmp_output0;
  size_t header_size0 = 0;
  std::vector<uint8_t> cmp_output1;
  size_t header_size1 = 0;
  CFG_COMPRESS::compress(input, input_size, cmp_output0, &header_size0, true,
                         debug);
  if (retry) {
    CFG_COMPRESS::compress(input, input_size, cmp_output1, &header_size1, false,
                           debug);
    if (debug) {
      printf("Compressed size (with followup byte support): %ld\n",
             cmp_output0.size());
      printf("Compressed size (without followup byte support): %ld\n",
             cmp_output1.size());
    }
  }
  if (retry && (cmp_output0.size() > cmp_output1.size())) {
    output.insert(output.end(), cmp_output1.begin(), cmp_output1.end());
  } else {
    output.insert(output.end(), cmp_output0.begin(), cmp_output0.end());
  }
  if (header_size != nullptr) {
    if (retry && (cmp_output0.size() > cmp_output1.size())) {
      (*header_size) = header_size1;
    } else {
      (*header_size) = header_size0;
    }
  }
}

void CFG_decompress(const uint8_t* input, const size_t input_size,
                    std::vector<uint8_t>& output, const bool debug) {
  CFG_ASSERT(input != nullptr);
  CFG_ASSERT(input_size > 0);
  CFG_COMPRESS::decompress(input, input_size, output, debug);
}

/*
  All about Messager
*/

void CFGMessager::add_msg(const std::string& m) {
  msgs.push_back(CFGMessage(CFGMessageType_INFO, m));
}

void CFGMessager::add_error(const std::string& m) {
  msgs.push_back(CFGMessage(CFGMessageType_ERROR, m));
}

void CFGMessager::append_error(const std::string& m) {
  msgs.push_back(CFGMessage(CFGMessageType_ERROR_APPEND, m));
}

void CFGMessager::clear() { msgs.clear(); }
