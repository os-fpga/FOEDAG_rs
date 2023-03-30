#include "BitGen_gemini.h"

const std::map<const std::string, const std::string> BOP_MAP = {
    {"fsbl", "BOP1"},
    {"uboot", "BOPU"},
    {"fcb", "BOPF"},
    {"icb", "BOPI"},
    {"pcb", "BOPP"}};

static const std::string& get_image_bop(const std::string& image) {
  auto iter = BOP_MAP.find(image);
  CFG_ASSERT(iter != BOP_MAP.end());
  return iter->second;
}

static const std::string get_bop_image(const std::string& bop) {
  for (auto& iter : BOP_MAP) {
    if (iter.second == bop) {
      return iter.first;
    }
  }
  CFG_INTERNAL_ERROR("Unsupport BOP image identifer %s", bop.c_str());
  return "";
}

// Implementation of Gemini BOP
BitGen_GEMINI_BOP_IMPL::BitGen_GEMINI_BOP_IMPL() {}

bool BitGen_GEMINI_BOP_IMPL::package(const std::string& image,
                                     std::vector<uint8_t>& data) {
  bool status = true;

  BitGen_GEMINI_BOP_DATA bop;
  std::vector<uint8_t> bop_data;

  // Setup all the info
  const std::string& bop_id = get_image_bop(image);
  bop.set_src("id", (uint8_t*)(const_cast<char*>(bop_id.c_str())),
              bop_id.size());
  bop.set_src("header_version", 1);
  bop.set_src("tool_version", 1);
  bop.set_src("binary_version", 1);
  bop.set_src("binary_length", uint64_t(data.size()));
  BitGen_DATA_RULE* offset_to_binary = bop.get_rule("offset_to_binary");
  CFG_ASSERT(offset_to_binary != nullptr);
  bop.set_src("offset_to_next_bop",
              offset_to_binary->default_value + uint64_t(data.size()));

  // Generate
  uint64_t bop_data_size = bop.generate(bop_data);
  CFG_ASSERT(bop_data_size > 0 && (bop_data_size % 32) == 0);

  // Package
  data.insert(data.begin(), bop_data.begin(), bop_data.end());
  return status;
}

// All FCB can be generated using generic function
// No customized implementation
BitGen_GEMINI_FCB_IMPL::BitGen_GEMINI_FCB_IMPL() {}

// All ICB_PACKET can be generated using generic function
// No customized implementation
BitGen_GEMINI_ICB_PACKET_IMPL::BitGen_GEMINI_ICB_PACKET_IMPL() {}

// All PCB_PACKET can be generated using generic function
// No customized implementation
BitGen_GEMINI_PCB_PACKET_IMPL::BitGen_GEMINI_PCB_PACKET_IMPL() {}

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
  BitGen_GEMINI_BOP_IMPL::package("fcb", fcb_data);
  data.insert(data.end(), fcb_data.begin(), fcb_data.end());

  // ICB
  const std::vector<std::vector<uint8_t>> icb_payloads = {
      {0, 1, 2, 3},
      {4, 5, 6, 7, 8, 9, 10, 11},
      {12, 13, 14, 15},
      {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}};
  std::vector<uint8_t> icb_data;
  CFG_append_u32(icb_data, (uint32_t)(icb_payloads.size()));
  size_t index = 0;
  for (auto& icb_payload : icb_payloads) {
    BitGen_GEMINI_ICB_PACKET_IMPL icb;
    std::vector<uint8_t> icb_packet_data;
    icb.set_src("src_payload", icb_payload);
    icb.set_src("is_data", index & 1);
    uint64_t icb_data_size = icb.generate(icb_packet_data);
    CFG_ASSERT(icb_data_size > 0 && (icb_data_size % 32) == 0);
    icb_data.insert(icb_data.end(), icb_packet_data.begin(),
                    icb_packet_data.end());
    index++;
  }
  BitGen_GEMINI_BOP_IMPL::package("icb", icb_data);
  data.insert(data.end(), icb_data.begin(), icb_data.end());

  // PCB
  std::vector<uint8_t> pcb_payload;
  for (uint32_t i = 0; i < (4096 * 3); i++) {
    CFG_append_u32(pcb_payload, i);
  }
  BitGen_GEMINI_PCB_PACKET_IMPL pcb;
  std::vector<uint8_t> pcb_data;
  pcb.set_src("src_payload", pcb_payload);
  uint64_t pcb_data_size = pcb.generate(pcb_data);
  CFG_ASSERT(pcb_data_size > 0 && (pcb_data_size % 32) == 0);
  BitGen_GEMINI_BOP_IMPL::package("pcb", pcb_data);
  data.insert(data.end(), pcb_data.begin(), pcb_data.end());

  return status;
}

void BitGen_GEMINI::parse(const std::string& input_filepath,
                          const std::string& output_filepath, bool detail) {
  std::vector<uint8_t> input_data;
  CFG_read_binary_file(input_filepath, input_data);
  std::ofstream file;
  file.open(output_filepath);
  CFG_ASSERT(file.is_open());
  CFG_ASSERT(input_data.size());

  uint64_t bit_index = 0;
  uint64_t total_bits = uint64_t(input_data.size() * 8);
  while (bit_index < total_bits) {
    BitGen_GEMINI_BOP_IMPL bop;
    uint64_t bop_data_size =
        bop.parse(file, &input_data[0], total_bits, bit_index, "", detail);
    CFG_ASSERT(bop_data_size > 0 && (bop_data_size % 32) == 0);
    std::vector<uint8_t>& id = bop.get_rule_data("id");
    const std::string image =
        get_bop_image(std::string((char*)(&id[0]), id.size()));
    uint64_t image_size = bop.get_rule_value("binary_length");
    file << "  ";
    for (uint8_t i = 0; i < 64; i++) {
      file << "*";
    }
    file << "\n";
    std::string space = "  * ";
    if (image == "fcb") {
      // FCB
      BitGen_GEMINI_FCB_IMPL fcb;
      uint64_t fcb_data_size =
          fcb.parse(file, &input_data[0], total_bits, bit_index, space, detail);
          file.flush();
      CFG_ASSERT(fcb_data_size > 0 && (fcb_data_size % 32) == 0);
      CFG_ASSERT(fcb_data_size == (image_size * 8));
    } else if (image == "icb") {
      // ICB
      uint32_t icb_packet_count =
          CFG_extract_bits(&input_data[0], total_bits, 32, bit_index);
      uint64_t icb_data_size = 32;
      file << CFG_print("%sICB count: %d\n", space.c_str(), icb_packet_count)
                  .c_str();
      for (uint32_t i = 0; i < icb_packet_count; i++) {
        file << CFG_print("%s  #%d\n", space.c_str(), i).c_str();
        BitGen_GEMINI_ICB_PACKET_IMPL icb;
        uint64_t icb_packet_size = icb.parse(file, &input_data[0], total_bits,
                                             bit_index, space + "    ", detail);
        file.flush();
        CFG_ASSERT(icb_packet_size > 0 && (icb_packet_size % 32) == 0);
        icb_data_size += icb_packet_size;
      }
      CFG_ASSERT(icb_data_size == (image_size * 8));
    } else if (image == "pcb") {
      // PCB
      BitGen_GEMINI_PCB_PACKET_IMPL pcb;
      uint64_t pcb_data_size =
          pcb.parse(file, &input_data[0], total_bits, bit_index, space, detail);
      file.flush();
      CFG_ASSERT(pcb_data_size > 0 && (pcb_data_size % 32) == 0);
      CFG_ASSERT(pcb_data_size == (image_size * 8));
    } else {
      CFG_INTERNAL_ERROR("Unsupport parsing for image %s", image.c_str());
    }
    file << "  ";
    for (uint8_t i = 0; i < 64; i++) {
      file << "*";
    }
    file << "\n";
  }
  CFG_ASSERT(bit_index == total_bits);

  // close file
  file.close();
}