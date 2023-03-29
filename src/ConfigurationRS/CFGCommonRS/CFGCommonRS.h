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

#endif
