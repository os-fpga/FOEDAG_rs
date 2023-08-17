#include "BitGen_gemini.h"

#include "BitGen_json.h"

BitGen_GEMINI::BitGen_GEMINI(const CFGObject_BITOBJ* bitobj)
    : m_bitobj(bitobj) {
  CFG_ASSERT(m_bitobj != nullptr);
}

uint64_t BitGen_GEMINI::convert_to(uint64_t value, uint64_t unit) {
  CFG_ASSERT(unit > 0);
  return ((value + (unit - 1)) / unit);
}

uint64_t BitGen_GEMINI::convert_to8(uint64_t value) {
  return convert_to(value, 8);
}

uint64_t BitGen_GEMINI::convert_to16(uint64_t value) {
  return convert_to(value, 16);
}

uint64_t BitGen_GEMINI::convert_to32(uint64_t value) {
  return convert_to(value, 32);
}

uint64_t BitGen_GEMINI::convert_to64(uint64_t value) {
  return convert_to(value, 64);
}

uint64_t BitGen_GEMINI::align(uint64_t value, uint64_t alignment) {
  return convert_to(value, alignment) * alignment;
}

void BitGen_GEMINI::generate(std::vector<BitGen_BITSTREAM_BOP*>& data) {
  BitGen_BITSTREAM_BOP* bop = CFG_MEM_NEW(BitGen_BITSTREAM_BOP);
  // Should use FPGA
  bop->field.identifier = "FPGA";
  bop->field.version = 0;
  bop->field.tool = "Raptor 1.0";
  bop->field.opn = m_bitobj->device;
  bop->field.jtag_id = 0;
  bop->field.jtag_mask = 0;
  bop->field.chipid = 0x10;
  bop->field.checksum = 0x10;
  bop->field.integrity = 0x10;
  // FCB data
  if (m_bitobj->configuration.protocol == "scan_chain") {
    CFG_ASSERT(m_bitobj->configuration.blwl.empty());
    CFG_ASSERT(m_bitobj->check_exist("scan_chain_fcb"));
    CFG_ASSERT(!m_bitobj->check_exist("ql_membank_fcb"));
    nlohmann::json fcb;
    fcb["action"] = "old_fcb_config";
    fcb["cfg_cmd"] = 1;
    fcb["bit_chain_connection"] = 0;
    fcb["bit_twist_shift_reg"] = 0;
    fcb["parallel_chains_count"] = m_bitobj->scan_chain_fcb.width;
    std::vector<uint8_t> payload = genbits_line_by_line(
        m_bitobj->scan_chain_fcb.data, m_bitobj->scan_chain_fcb.width,
        m_bitobj->scan_chain_fcb.length, 8, 32,
        fcb["bit_chain_connection"] != 0, fcb["bit_twist_shift_reg"] != 0);
    fcb["payload"] = nlohmann::json(payload);
    bop->actions.push_back(BitGen_JSON::gen_old_fcb_config_action(fcb));
    BitGen_JSON::zeroize_array_numbers(fcb["payload"]);
    memset(&payload[0], 0, payload.size());
    data.push_back(bop);
  } else {
    CFG_ASSERT(m_bitobj->configuration.protocol == "ql_memory_bank");
    CFG_ASSERT(m_bitobj->configuration.blwl == "flatten");
    CFG_ASSERT(!m_bitobj->check_exist("scan_chain_fcb"));
    CFG_ASSERT(m_bitobj->check_exist("ql_membank_fcb"));
    CFG_ASSERT(m_bitobj->ql_membank_fcb.bl);
    CFG_ASSERT(m_bitobj->ql_membank_fcb.wl);
    nlohmann::json fcb;
    fcb["action"] = "fcb_config";
    fcb["bitline_byte_size"] = ((m_bitobj->ql_membank_fcb.bl + 31) / 32) * 4;
    fcb["readback"] = 0;
    uint32_t bl_byte_size = (m_bitobj->ql_membank_fcb.bl + 7) / 8;
    CFG_ASSERT((uint32_t)(m_bitobj->ql_membank_fcb.data.size()) ==
               (m_bitobj->ql_membank_fcb.wl * bl_byte_size));
    std::vector<uint8_t> payload;
    uint32_t padding = 0;
    for (uint32_t wl = 0, index = 0; wl < m_bitobj->ql_membank_fcb.wl;
         wl++, index += bl_byte_size) {
      payload.insert(
          payload.end(), m_bitobj->ql_membank_fcb.data.begin() + index,
          m_bitobj->ql_membank_fcb.data.begin() + index + bl_byte_size);
      padding = 0;
      while ((bl_byte_size + padding) % 4) {
        payload.push_back(0);
        padding++;
      }
    }
    fcb["payload"] = nlohmann::json(payload);
    bop->actions.push_back(BitGen_JSON::gen_fcb_config_action(fcb));
    BitGen_JSON::zeroize_array_numbers(fcb["payload"]);
    memset(&payload[0], 0, payload.size());
    data.push_back(bop);
  }
  // ICB data
  // PCB data
}

std::vector<uint8_t> BitGen_GEMINI::genbits_line_by_line(
    const std::vector<uint8_t>& src_data, uint64_t line_bits,
    uint64_t total_line, uint64_t src_unit_bits, uint64_t dest_unit_bits,
    bool pad_reversed /*bit_chain_connection*/,
    bool unit_reversed /*bit_twist_shift_reg*/) {
  std::vector<uint8_t> dest_data;
  CFG_ASSERT(line_bits > 0);
  CFG_ASSERT(total_line > 0);
  CFG_ASSERT(src_unit_bits > 0);
  CFG_ASSERT(dest_unit_bits > 0);
  uint64_t src_line_aligned_bits = align(line_bits, src_unit_bits);
  uint64_t dest_line_aligned_bits = align(line_bits, dest_unit_bits);
  CFG_ASSERT((src_line_aligned_bits * total_line) == (src_data.size() * 8));
  uint64_t padding_bits = dest_line_aligned_bits - line_bits;
  CFG_ASSERT(padding_bits < dest_unit_bits);
  dest_data.resize(convert_to8(dest_line_aligned_bits * total_line));
  memset(&dest_data[0], 0, dest_data.size());
  for (uint64_t line = 0; line < total_line; line++) {
    size_t src_index = size_t(line * src_line_aligned_bits);
    size_t dest_index = size_t(line * dest_line_aligned_bits);
    size_t original_dest_index = dest_index;
    size_t end_dest_index = original_dest_index + dest_line_aligned_bits;
    if (unit_reversed) {
      dest_index += (dest_line_aligned_bits - dest_unit_bits);
    }
    if (pad_reversed) {
      dest_index += padding_bits;
    }
    for (uint64_t bit = 0; bit < line_bits; bit++) {
      if (src_data[src_index >> 3] & (1 << (src_index & 7))) {
        dest_data[dest_index >> 3] |= (1 << (dest_index & 7));
      }
      src_index++;
      dest_index++;
      if (unit_reversed && bit < (line_bits - 1) &&
          (dest_index % dest_unit_bits) == 0) {
        dest_index -= (2 * dest_unit_bits);
        CFG_ASSERT(dest_index >= original_dest_index);
        CFG_ASSERT(dest_index < end_dest_index);
      }
    }
  }
  return dest_data;
}