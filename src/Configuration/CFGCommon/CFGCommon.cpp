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

#include "CFGHelper.h"
#include "CFGMessager.h"

/*
  All about Helper
*/

std::string CFG_print
(
  const char * format_string,
  ...
) {
  char * buf = nullptr;
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
      memcpy((char *)(&string[0]), buf, n);
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

void CFG_assertion
(
  const char * file,
  size_t line,
  std::string msg
) {
  printf("Assertion at %s (line: %ld)\n", file, line);
  printf("   MSG: %s\n\n", msg.c_str());
  exit(-1);
}

std::string CFG_get_time() {
  time_t rawtime;
  struct tm * timeinfo;
  char buffer[80];
  time (&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer,sizeof(buffer), "%d %b %Y %H:%M:%S", timeinfo);
  std::string str(buffer);
  return str;
}

void CFG_get_rid_trailing_whitespace
(
  std::string& string,
  const std::vector<char> whitespaces
) {
  CFG_ASSERT(whitespaces.size());
  while (string.size()) {
    auto iter = std::find(whitespaces.begin(), whitespaces.end(), string.back());
    if (iter != whitespaces.end()) {
      string.pop_back();
    } else {
      break;
    }
  }
}

void CFG_get_rid_leading_whitespace
(
  std::string& string,
  const std::vector<char> whitespaces
) {
  CFG_ASSERT(whitespaces.size());
  while (string.size()) {
    auto iter = std::find(whitespaces.begin(), whitespaces.end(), string.front());
    if (iter != whitespaces.end()) {
      string.erase(0, 1);
    } else {
      break;
    }
  }
}

void CFG_get_rid_whitespace
(
  std::string& string,
  const std::vector<char> whitespaces
) {
  CFG_get_rid_trailing_whitespace(string, whitespaces);
  CFG_get_rid_leading_whitespace(string, whitespaces);
}

CFG_TIME CFG_time_begin() {
  return std::chrono::high_resolution_clock::now();
}

uint64_t CFG_nano_time_elapse(CFG_TIME begin) {
  CFG_TIME end = std::chrono::high_resolution_clock::now();
  std::chrono::nanoseconds elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
  return (uint64_t)(elapsed.count());
}

float CFG_time_elapse(CFG_TIME begin) {
  return float(CFG_nano_time_elapse(begin) * 1e-9);
}

/*
  All about Messager
*/

void CFGMessager::add_msg(const std::string &m) {
  msgs.push_back(CFGMessage(CFGMessageType::INFO, m));
}

void CFGMessager::add_error(const std::string &m) {
  msgs.push_back(CFGMessage(CFGMessageType::ERROR, m));
}

void CFGMessager::append_error(const std::string &m) {
  msgs.push_back(CFGMessage(CFGMessageType::ERROR_APPEND, m));
}

void CFGMessager::clear() {
  msgs.clear();
}
