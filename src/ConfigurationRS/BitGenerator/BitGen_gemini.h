#ifndef BITGEN_GEMINI_H
#define BITGEN_GEMINI_H

#include "BitGen_packer.h"
#include "CFGObject/CFGObject_auto.h"

class BitGen_GEMINI {
 public:
  BitGen_GEMINI(const CFGObject_BITOBJ* bitobj);
  void generate(std::vector<BitGen_BITSTREAM_BOP*>& data);

 protected:
  uint64_t convert_to(uint64_t value, uint64_t unit);
  uint64_t convert_to8(uint64_t value);
  uint64_t convert_to16(uint64_t value);
  uint64_t convert_to32(uint64_t value);
  uint64_t convert_to64(uint64_t value);
  uint64_t align(uint64_t value, uint64_t alignment);
  std::vector<uint8_t> genbits_line_by_line(
      const std::vector<uint8_t>& src_data, uint64_t line_bits,
      uint64_t total_line, uint64_t src_unit_bits, uint64_t dest_unit_bits,
      bool pad_reversed, bool unit_reversed);
  void get_pcb_user_data_and_parity(std::vector<uint8_t>& data,
                                    std::vector<uint8_t>& user_data,
                                    std::vector<uint8_t>& parity);
  void get_pcb_xy_offset_stride(const std::vector<CFGObject_BITOBJ_PCB*>& pcbs,
                                uint32_t& row_offset, uint32_t& row_stride,
                                uint32_t& col_offset, uint32_t& col_stride);
  bool get_data_bit(std::vector<uint8_t>& data, size_t index);
  void append_assign_data_bit(std::vector<uint8_t>& data, bool value,
                              size_t index);
  void adjust_data_bit_size(std::vector<uint8_t>& data, size_t original_size,
                            size_t new_size, size_t count,
                            bool value_to_adjust = false,
                            bool include_remaining = false);
  void append_alternate_data(std::vector<uint8_t>& data,
                             std::vector<uint8_t>& data0,
                             std::vector<uint8_t>& data1, size_t bit_size,
                             size_t count, size_t dest_index,
                             bool check_byte_alignment);

 private:
  const CFGObject_BITOBJ* m_bitobj;
};

#endif