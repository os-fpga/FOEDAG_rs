#ifndef CFGHelper_H
#define CFGHelper_H

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

// Assertion
#define CFG_PRINT_MINIMUM_SIZE (20)
#define CFG_PRINT_MAXIMUM_SIZE (8192)
typedef std::chrono::high_resolution_clock::time_point CFG_TIME;

std::string CFG_print(const char* format_string, ...);

void CFG_assertion(const char* file, size_t line, std::string msg);

#define CFG_INTERNAL_ERROR(...) \
  { CFG_assertion(__FILE__, __LINE__, CFG_print(__VA_ARGS__)); }

#if defined(_MSC_VER)

#define CFG_ASSERT(truth)                      \
  if (!(truth)) {                              \
    CFG_assertion(__FILE__, __LINE__, #truth); \
  }

#define CFG_ASSERT_MSG(truth, ...)                             \
  if (!(truth)) {                                              \
    CFG_assertion(__FILE__, __LINE__, CFG_print(__VA_ARGS__)); \
  }

#else

#define CFG_ASSERT(truth)                      \
  if (!(__builtin_expect(!!(truth), 0))) {     \
    CFG_assertion(__FILE__, __LINE__, #truth); \
  }

#define CFG_ASSERT_MSG(truth, ...)                             \
  if (!(__builtin_expect(!!(truth), 0))) {                     \
    CFG_assertion(__FILE__, __LINE__, CFG_print(__VA_ARGS__)); \
  }

#endif

std::string CFG_get_time();

void CFG_get_rid_trailing_whitespace(std::string& string,
                                     const std::vector<char> whitespaces = {
                                         ' ', '\t', '\n', '\r'});

void CFG_get_rid_leading_whitespace(std::string& string,
                                    const std::vector<char> whitespaces = {
                                        ' ', '\t', '\n', '\r'});

void CFG_get_rid_whitespace(std::string& string,
                            const std::vector<char> whitespaces = {' ', '\t',
                                                                   '\n', '\r'});

CFG_TIME CFG_time_begin();

uint64_t CFG_nano_time_elapse(CFG_TIME begin);

float CFG_time_elapse(CFG_TIME begin);

uint8_t CFG_write_variable_u64(std::vector<uint8_t>& data, uint64_t value);

uint64_t CFG_read_variable_u64(const uint8_t* data, size_t data_size,
                               size_t& index, int max_size = -1);

void CFG_read_binary_file(const std::string& filepath,
                          std::vector<uint8_t>& data);

void CFG_write_binary_file(const std::string& filepath, const uint8_t* data,
                           const size_t data_size);

void CFG_compress(const uint8_t* input, const size_t input_size,
                  std::vector<uint8_t>& output, size_t* header_size = nullptr,
                  const bool debug = false, const bool retry = true);

void CFG_decompress(const uint8_t* input, const size_t input_size,
                    std::vector<uint8_t>& output, const bool debug = false);

#endif
