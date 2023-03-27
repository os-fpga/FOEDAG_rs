#include "BitGen_gemini.h"

// All FCB can be generated using generic function
// No customized implementation
BitGen_GEMINI_FCB_IMPL::BitGen_GEMINI_FCB_IMPL() {}

// All ICB_PACKET can be generated using generic function
// No customized implementation
BitGen_GEMINI_ICB_PACKET_IMPL::BitGen_GEMINI_ICB_PACKET_IMPL() {}

BitGen_GEMINI::BitGen_GEMINI(const CFGObject_BITOBJ* bitobj)
    : m_bitobj(bitobj) {
  CFG_ASSERT(m_bitobj != nullptr);
}

bool BitGen_GEMINI::generate(std::vector<uint8_t>& data) {
  bool status = true;
  // cleanup
  data.clear();

  // FCB
  BitGen_GEMINI_FCB_IMPL fcb;
  std::vector<uint8_t> fcb_data;
  fcb.set_src("parallel_chain_size", m_bitobj->fcb.width);
  fcb.set_src("bit_size_per_chain", m_bitobj->fcb.length);
  fcb.set_src("src_payload", m_bitobj->fcb.data);
  uint64_t fcb_data_size = fcb.generate(fcb_data);
  CFG_ASSERT(fcb_data_size > 0 && (fcb_data_size % 32) == 0);
  data.insert(data.end(), fcb_data.begin(), fcb_data.end());

  // ICB
  const std::vector<std::vector<uint8_t>> icb_packets = {
      {0, 1, 2, 3},
      {4, 5, 6, 7, 8, 9, 10, 11},
      {12, 13, 14, 15},
      {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}};
  CFG_append_u32(data, (uint32_t)(icb_packets.size()));
  size_t index = 0;
  for (auto& icb_packet : icb_packets) {
    BitGen_GEMINI_ICB_PACKET_IMPL icb;
    std::vector<uint8_t> icb_data;
    icb.set_src("src_payload", icb_packet);
    icb.set_src("is_data", index & 1);
    uint64_t icb_data_size = icb.generate(icb_data);
    CFG_ASSERT(icb_data_size > 0 && (icb_data_size % 32) == 0);
    data.insert(data.end(), icb_data.begin(), icb_data.end());
    index++;
  }

  // PCB
  return status;
}

uint64_t BitGen_GEMINI::parse(std::ofstream& file, const uint8_t* data,
                              uint64_t total_bit_size, uint64_t& bit_index,
                              std::string space, const bool detail) {
  CFG_ASSERT(file.is_open());
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(total_bit_size > 0);
  CFG_ASSERT(bit_index < total_bit_size);

  uint64_t original_bit_index = bit_index;

  // FCB
  BitGen_GEMINI_FCB_IMPL fcb;
  uint64_t fcb_data_size =
      fcb.parse(file, data, total_bit_size, bit_index, space, detail);
  CFG_ASSERT(fcb_data_size > 0 && (fcb_data_size % 32) == 0);

  // ICB
  uint32_t icb_packet_count =
      CFG_extract_bits(data, total_bit_size, 32, bit_index);
  file << CFG_print("%sICB count: %d\n", space.c_str(), icb_packet_count)
              .c_str();
  for (uint32_t i = 0; i < icb_packet_count; i++) {
    file << CFG_print("%s  #%d\n", space.c_str(), i).c_str();
    BitGen_GEMINI_ICB_PACKET_IMPL icb;
    uint64_t icb_data_size = icb.parse(file, data, total_bit_size, bit_index,
                                       space + "    ", detail);
    CFG_ASSERT(icb_data_size > 0 && (icb_data_size % 32) == 0);
  }

  // PCB

  // Return total bit
  return (bit_index - original_bit_index);
}