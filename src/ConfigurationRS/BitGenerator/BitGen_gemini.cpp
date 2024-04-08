#include "BitGen_gemini.h"

#include "BitGen_json.h"

#define PCB_BIT_SIZE (36 * 1024)
#define PCB_PAYLOAD_BIT_SIZE (32 * 1024)
#define PCB_PARITY_BIT_SIZE (4 * 1024)

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
  // ToDO: Raptor Version need to be runtime determined
  bop->field.opn_tool = CFG_print("%s (Raptor 1.0)", m_bitobj->device.c_str());
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
    payload.clear();
  }
  // ICB data
  if (m_bitobj->icb.bits) {
    CFG_ASSERT(m_bitobj->icb.data.size() ==
               (size_t)((m_bitobj->icb.bits + 7) / 8));
    nlohmann::json icb;
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), m_bitobj->icb.data.begin(),
                   m_bitobj->icb.data.end());
    while (payload.size() % 4) {
      payload.push_back(0);
    }
    icb["action"] = "icb_config";
    icb["cfg_cmd"] = 0;
    icb["bit_twist"] = 0;
    icb["byte_twist"] = 0;
    icb["is_data_or_not_cmd"] = 0;
    icb["update"] = 0;
    icb["capture"] = 0;
    icb["payload"] = nlohmann::json(payload);
    bop->actions.push_back(BitGen_JSON::gen_icb_config_action(icb));
    BitGen_JSON::zeroize_array_numbers(icb["payload"]);
    memset(&payload[0], 0, payload.size());
    payload.clear();
  } else {
    CFG_ASSERT(m_bitobj->icb.data.size() == 0);
  }

  // PCB data
  if (m_bitobj->pcb.size()) {
    uint32_t row_offset = 0;
    uint32_t row_stride = 0;
    uint32_t col_offset = 0;
    uint32_t col_stride = 0;
    get_pcb_xy_offset_stride(m_bitobj->pcb, row_offset, row_stride, col_offset,
                             col_stride);
#if 1
    // Send all block data in one action
    nlohmann::json pcb;
    pcb["action"] = "pcb_config";
    pcb["ram_block_count"] = (uint32_t)(m_bitobj->pcb.size());
    pcb["pl_ctl_skew"] = 3;
    pcb["pl_ctl_parity"] = 0;
    pcb["pl_ctl_even"] = 0;
    pcb["pl_ctl_split"] = 0;
    pcb["pl_select_offset"] = 0;
    pcb["pl_select_row"] = row_offset;
    pcb["pl_select_col"] = col_offset;
    pcb["pl_row_offset"] = row_offset;
    pcb["pl_row_stride"] = row_stride;
    pcb["pl_col_offset"] = col_offset;
    pcb["pl_col_stride"] = col_stride;
    pcb["pl_extra_w32"] = 0;
    pcb["pl_extra_w33"] = 0;
    pcb["pl_extra_w34"] = 0;
    pcb["pl_extra_w35"] = 0;
    std::vector<uint8_t> payload;
    for (auto& pcbobj : m_bitobj->pcb) {
      CFG_ASSERT(pcbobj->bits == PCB_BIT_SIZE);
      CFG_ASSERT(pcbobj->data.size() == (PCB_BIT_SIZE + 7) / 8);
      std::vector<uint8_t> block_payload;
      std::vector<uint8_t> block_parity;
      get_pcb_payload_and_parity(pcbobj->data, block_payload, block_parity);
      payload.insert(payload.end(), block_payload.begin(), block_payload.end());
      memset(&block_payload[0], 0, block_payload.size());
      memset(&block_parity[0], 0, block_parity.size());
      block_payload.clear();
      block_parity.clear();
    }
    pcb["payload"] = nlohmann::json(payload);
    bop->actions.push_back(BitGen_JSON::gen_pcb_config_action(pcb));
    BitGen_JSON::zeroize_array_numbers(pcb["payload"]);
    memset(&payload[0], 0, payload.size());
    payload.clear();
#else
    // Send block data in individual action
    for (auto& pcbobj : m_bitobj->pcb) {
      CFG_ASSERT(pcbobj->bits == PCB_BIT_SIZE);
      CFG_ASSERT(pcbobj->data.size() == (PCB_BIT_SIZE + 7) / 8);
      std::vector<uint8_t> payload;
      std::vector<uint8_t> parity;
      get_pcb_payload_and_parity(pcbobj->data, payload, parity);
      nlohmann::json pcb;
      pcb["action"] = "pcb_config";
      pcb["ram_block_count"] = 1;
      // https://github.com/RapidSilicon/virgo/blob/060ab0e60de9d0f45fb875cc09c04bec0781861e/DV/virgo_verif_env/bcpu_real_core_c_tests/IPs/PCB/bcpu_real_pcb_a_inc_test/program.c#L20-L21
      pcb["pl_ctl_skew"] = 3;
      pcb["pl_ctl_parity"] = 0;
      pcb["pl_ctl_even"] = 0;
      pcb["pl_ctl_split"] = 0;
      pcb["pl_select_offset"] = 0;
      pcb["pl_select_row"] = pcbobj->y;
      pcb["pl_select_col"] = pcbobj->x;
      // https://github.com/RapidSilicon/virgo/blob/main/DV/virgo_verif_env/bcpu_real_core_c_tests/IPs/PCB/program.h#L10-L13
      pcb["pl_row_offset"] = row_offset;
      pcb["pl_row_stride"] = row_stride;
      pcb["pl_col_offset"] = col_offset;
      pcb["pl_col_stride"] = col_stride;
      pcb["pl_extra_w32"] = 0;
      pcb["pl_extra_w33"] = 0;
      pcb["pl_extra_w34"] = 0;
      pcb["pl_extra_w35"] = 0;
      pcb["payload"] = nlohmann::json(payload);
      bop->actions.push_back(BitGen_JSON::gen_pcb_config_action(pcb));
      BitGen_JSON::zeroize_array_numbers(pcb["payload"]);
      memset(&payload[0], 0, payload.size());
      memset(&parity[0], 0, parity.size());
      payload.clear();
      parity.clear();
    }
#endif
  }

  // Finalize
  data.push_back(bop);
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

void BitGen_GEMINI::get_pcb_payload_and_parity(std::vector<uint8_t>& data,
                                               std::vector<uint8_t>& payload,
                                               std::vector<uint8_t>& parity) {
  CFG_ASSERT(data.size() == (PCB_BIT_SIZE + 7) / 8);
  if (parity.size()) {
    memset(&payload[0], 0, payload.size());
    payload.clear();
  }
  if (parity.size()) {
    memset(&parity[0], 0, parity.size());
    parity.clear();
  }
  payload.resize((PCB_PAYLOAD_BIT_SIZE + 7) / 8);
  parity.resize((PCB_PARITY_BIT_SIZE + 7) / 8);
  memset(&payload[0], 0, payload.size());
  memset(&parity[0], 0, parity.size());
  uint8_t* data0 = &data[0];
  uint8_t* data1 = &data[((PCB_PAYLOAD_BIT_SIZE + 7) / 8) / 2];
  uint32_t index = 0;
  uint32_t half_index = 0;
  uint32_t payload_index = 0;
  uint32_t parity_index = 0;
  for (uint32_t i = 0; i < 1024; i++) {
    uint32_t temp = half_index;
    for (uint32_t j = 0; j < 18; j++, index++, temp++) {
      if (j < 16) {
        if (data0[temp >> 3] & (1 << (temp & 7))) {
          payload[payload_index >> 3] |= (1 << (payload_index & 7));
        }
        payload_index++;
      } else {
        if (data0[temp >> 3] & (1 << (temp & 7))) {
          parity[parity_index >> 3] |= (1 << (parity_index & 7));
        }
        parity_index++;
      }
    }
    temp = half_index;
    for (uint32_t j = 0; j < 18; j++, index++, temp++, half_index++) {
      if (j < 16) {
        if (data1[temp >> 3] & (1 << (temp & 7))) {
          payload[payload_index >> 3] |= (1 << (payload_index & 7));
        }
        payload_index++;
      } else {
        if (data1[temp >> 3] & (1 << (temp & 7))) {
          parity[parity_index >> 3] |= (1 << (parity_index & 7));
        }
        parity_index++;
      }
    }
  }
  CFG_ASSERT(index == PCB_BIT_SIZE);
  CFG_ASSERT(half_index == (PCB_BIT_SIZE / 2));
  CFG_ASSERT(payload_index == PCB_PAYLOAD_BIT_SIZE);
  CFG_ASSERT(parity_index == PCB_PARITY_BIT_SIZE);
}

void BitGen_GEMINI::get_pcb_xy_offset_stride(
    const std::vector<CFGObject_BITOBJ_PCB*>& pcbs, uint32_t& row_offset,
    uint32_t& row_stride, uint32_t& col_offset, uint32_t& col_stride) {
  row_offset = 0;
  row_stride = 0;
  col_offset = 0;
  col_stride = 0;
  CFG_ASSERT(pcbs.size());
  std::vector<uint32_t> cols;
  std::vector<uint32_t> rows;
  for (auto& pcb : pcbs) {
    if (std::find(cols.begin(), cols.end(), pcb->x) == cols.end()) {
      cols.push_back(pcb->x);
    }
    if (std::find(rows.begin(), rows.end(), pcb->y) == rows.end()) {
      rows.push_back(pcb->y);
    }
  }
  CFG_ASSERT(cols.size());
  CFG_ASSERT(rows.size());
  std::sort(cols.begin(), cols.end(),
            [](uint32_t a, uint32_t b) { return a < b; });
  std::sort(rows.begin(), rows.end(),
            [](uint32_t a, uint32_t b) { return a < b; });
  std::string xmsg = "";
  std::string ymsg = "";
  for (auto t : cols) {
    if (xmsg.size()) {
      xmsg = CFG_print("%s, %d", xmsg.c_str(), t);
    } else {
      xmsg = CFG_print("%d", t);
    }
  }
  for (auto t : rows) {
    if (ymsg.size()) {
      ymsg = CFG_print("%s, %d", ymsg.c_str(), t);
    } else {
      ymsg = CFG_print("%d", t);
    }
  }
#if 0
  CFG_POST_MSG("PCB Col Stride sequence: %s\n", xmsg.c_str());
  CFG_POST_MSG("PCB Row Stride sequence: %s\n", ymsg.c_str());
#endif
  row_offset = rows[0];
  col_offset = cols[0];
  if (rows.size() > 1) {
    size_t diff = 0;
    for (size_t i = 0, j = 1; j < rows.size(); i++, j++) {
      CFG_ASSERT(rows[i] < rows[j]);
      if (diff == 0) {
        diff = (rows[j] - rows[i]);
      } else if (diff != (rows[j] - rows[i])) {
        CFG_POST_WARNING("Invalid PCB Row Stride sequence: %s", ymsg.c_str());
        break;
      }
    }
    row_stride = rows[1] - rows[0];
  }
  if (cols.size() > 1) {
    size_t diff = 0;
    for (size_t i = 0, j = 1; j < cols.size(); i++, j++) {
      CFG_ASSERT(cols[i] < cols[j]);
      if (diff == 0) {
        diff = (cols[j] - cols[i]);
      } else if (diff != (cols[j] - cols[i])) {
        CFG_POST_WARNING("Invalid PCB Col Stride sequence: %s", xmsg.c_str());
        break;
      }
    }
    col_stride = cols[1] - cols[0];
  }
}
