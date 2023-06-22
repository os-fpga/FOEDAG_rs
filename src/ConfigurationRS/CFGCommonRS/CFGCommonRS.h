#ifndef CFGCommonRS_H
#define CFGCommonRS_H

#include <fstream>
#include <iostream>
#include <vector>

#include "Configuration/CFGCommon/CFGCommon.h"

uint8_t CFG_write_variable_u64(std::vector<uint8_t>& data, uint64_t value);

uint64_t CFG_read_variable_u64(const uint8_t* data, size_t data_size,
                               size_t& index, int max_size = -1);

void CFG_read_binary_file(const std::string& filepath,
                          std::vector<uint8_t>& data);

void CFG_write_binary_file(const std::string& filepath, const uint8_t* data,
                           const size_t data_size);

std::vector<uint8_t> CFG_convert_hex_string_to_bytes(std::string string,
                                                     bool no_empty = false,
                                                     bool* status = NULL);

std::string CFG_convert_bytes_to_hex_string(uint8_t* data, size_t size,
                                            std::string delimiter = "",
                                            bool hex_prefix = false);

void CFG_compress(const uint8_t* input, const size_t input_size,
                  std::vector<uint8_t>& output, size_t* header_size = nullptr,
                  const bool debug = false, const bool retry = true);

void CFG_decompress(const uint8_t* input, const size_t input_size,
                    std::vector<uint8_t>& output, const bool debug = false);

uint16_t CFG_crc16(const uint8_t* addr, size_t size,
                   uint16_t lfsr_init = static_cast<uint16_t>(-1),
                   bool final_xor = true,
                   const uint16_t* custom_table = nullptr);

uint16_t CFG_bop_A001_crc16(const uint8_t* addr, size_t size,
                            uint16_t lfsr_init = 0, bool final_xor = false,
                            const uint16_t* custom_table = nullptr);

uint32_t CFG_crc32(const uint8_t* addr, size_t size,
                   uint32_t lfsr_init = static_cast<uint32_t>(-1),
                   bool final_xor = true,
                   const uint32_t* custom_table = nullptr);

void CFG_append_u8(std::vector<uint8_t>& data, uint8_t value);

void CFG_append_u16(std::vector<uint8_t>& data, uint16_t value);

void CFG_append_u32(std::vector<uint8_t>& data, uint32_t value);

void CFG_append_u64(std::vector<uint8_t>& data, uint64_t value);

uint64_t CFG_extract_bits(const uint8_t* data, const uint64_t total_bit_size,
                          const uint32_t bit_size, uint64_t& bit_index);

int CFG_check_file_extensions(const std::string& filepath,
                              const std::vector<std::string> extensions);

void CFG_print_hex(std::ofstream& file, const uint8_t* data,
                   const uint64_t data_size, const uint8_t unit_size,
                   const std::string space, bool detail);

void CFG_print_binary_line_by_line(std::ofstream& file, const uint8_t* data,
                                   const uint64_t total_bits,
                                   const uint64_t bit_per_line,
                                   const uint64_t data_alignment,
                                   const std::string space, bool detail);

std::string CFG_get_machine_name();

uint32_t CFG_get_volume_serial_number();

uint64_t CFG_get_nano_time();

uint64_t CFG_get_unique_nano_time();

void CFG_TRACK_MEM(void* ptr, const char* filename, size_t line);
void CFG_UNTRACK_MEM(void* ptr, const char* filename, size_t line);

#if 1
template <typename T>
inline T* CFG_mem_new_function(T* ptr) {
  CFG_ASSERT(ptr != nullptr);
  CFG_TRACK_MEM(ptr, __FILENAME__, __LINE__);
  return ptr;
}
#define CFG_MEM_NEW(T, ...) CFG_mem_new_function<T>(new T(__VA_ARGS__));
#else
// This easier way work for GCC but not MSVC
#define CFG_MEM_NEW(CLASS, ...)                 \
  ({                                            \
    CLASS* ptr = new CLASS(__VA_ARGS__);        \
    CFG_ASSERT(ptr != nullptr);                 \
    CFG_TRACK_MEM(ptr, __FILENAME__, __LINE__); \
    ptr;                                        \
  })
#endif

#define CFG_MEM_DELETE(ptr)                       \
  if (ptr != nullptr) {                           \
    CFG_UNTRACK_MEM(ptr, __FILENAME__, __LINE__); \
    delete ptr;                                   \
  }

#endif
