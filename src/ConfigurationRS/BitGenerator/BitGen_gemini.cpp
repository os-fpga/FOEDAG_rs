#include "BitGen_gemini.h"

#define BOP_HEADER_SIZE (0x40)

const std::map<const std::string, const std::string> BOP_MAP = {
    {"fsbl", "FSBL"},
    {"uboot", "UBOT"},
    {"fcb", "FCB"},
    {"icb", "ICB"},
    {"pcb", "PCB"}};

static std::string get_image_bop(const std::string& image) {
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

// Implementation of Gemini BOP Header
BitGen_GEMINI_BOP_HEADER_IMPL::BitGen_GEMINI_BOP_HEADER_IMPL() {}

bool BitGen_GEMINI_BOP_HEADER_IMPL::package(const std::string& image,
                                            std::vector<uint8_t>& data,
                                            bool compress, CFGCrypto_KEY*& key,
                                            const size_t aes_key_size,
                                            const size_t header_size) {
  CFG_ASSERT(aes_key_size == 0 || aes_key_size == 16 || aes_key_size == 32);
  CFG_ASSERT(header_size > 0 && (header_size % 4) == 0);
  CFG_ASSERT(data.size() > header_size);
  bool status = true;

  BitGen_GEMINI_BOP_HEADER_DATA bop;
  std::vector<uint8_t> bop_data;

  // Setup all the info
  std::string bop_id = get_image_bop(image);
  CFG_ASSERT(bop_id.size() > 0 && bop_id.size() <= 4);
  while (bop_id.size() < 4) {
    bop_id.push_back(0);
  }
  bop.set_src("id", (uint8_t*)(const_cast<char*>(bop_id.c_str())),
              bop_id.size());
  bop.set_src("header_version", 1);
  bop.set_src("tool_version", 1);
  bop.set_src("binary_version", 1);
  // word 4 (binary length include binary padding but not xcb header)
  bop.set_src("binary_length", uint64_t(data.size() - header_size));
  // word 7 (BOP header + xcb header)
  bop.set_src("offset_to_binary", BOP_HEADER_SIZE + header_size);
  // Next BOP should be
  //   1. BOP header
  //   2. Binary
  //   3. IV
  //   4. pub key (alignment to 16)
  //   5. signature
  //   6. xcb header (which is already including in our data.size())
  uint64_t iv_size = (aes_key_size == 0 ? 0 : 16);
  uint64_t pub_key_size =
      (key == nullptr ? 0 : (uint64_t)(key->get_public_key_size(0)));
  uint64_t aligned_pub_key_size =
      (key == nullptr ? 0 : (uint64_t)(key->get_public_key_size(16)));
  uint64_t signature_size =
      (key == nullptr ? 0 : (uint64_t)(key->get_signature_size()));
  // word 8
  bop.set_src("offset_to_next_bop",
              BOP_HEADER_SIZE + uint64_t(data.size()) + iv_size +
                  uint64_t(aligned_pub_key_size) + signature_size);
  // word 9
  bop.set_src("signing_algo",
              key == nullptr ? 0 : uint64_t(key->get_bitstream_signing_algo()));
  bop.set_src("encryption_algo", aes_key_size == 0 ? 0 : 1);
  bop.set_src("iv_length", iv_size);
  // word 10
  bop.set_src("public_key_length", pub_key_size);
  bop.set_src("encryption_length", aes_key_size);
  // word 11 (FPGA binary does not support binary padding)
  bop.set_src("signature_length", signature_size);
  bop.set_src("compression_algo", compress ? 2 : 0);
  // word 12
  bop.set_src("xcb_header_size", header_size);
  // Generate - never need to encrypt header
  std::vector<uint8_t> empty_aes_key;
  bool bitgen_compress = false;
  uint64_t bop_data_size =
      bop.generate(bop_data, bitgen_compress, empty_aes_key, nullptr);
  CFG_ASSERT(bop_data_size > 0 && (bop_data_size % 32) == 0);
  CFG_ASSERT(bop_data_size == (BOP_HEADER_SIZE * 8));

  // Package
  data.insert(data.begin(), bop_data.begin(), bop_data.end());
  return status;
}

// Implementation of Gemini BOP Footer
BitGen_GEMINI_BOP_FOOTER_IMPL::BitGen_GEMINI_BOP_FOOTER_IMPL() {}

bool BitGen_GEMINI_BOP_FOOTER_IMPL::package(std::vector<uint8_t>& data,
                                            CFGCrypto_KEY*& key,
                                            const size_t aes_key_size,
                                            const uint8_t* iv) {
  CFG_ASSERT(aes_key_size == 0 || aes_key_size == 16 || aes_key_size == 32);
  bool status = true;

  if (aes_key_size != 0 || key != nullptr) {
    BitGen_GEMINI_BOP_FOOTER_DATA bop;
    std::vector<uint8_t> bop_data;
    std::vector<uint8_t> public_key;
    std::vector<uint8_t> signature;

    // Setup all the info
    if (aes_key_size != 0) {
      bop.set_rule_size("iv", 128);
      bop.set_src("iv", iv, 16);
    }
    if (key != nullptr) {
      key->get_public_key(public_key, 16);
      CFG_ASSERT(public_key.size());
      bop.set_rule_size("public_key", public_key.size() * 8);
      bop.set_src("public_key", public_key);
      signature.resize(key->get_signature_size());
      CFG_ASSERT(signature.size());
      bop.set_rule_size("signature", signature.size() * 8);
      bop.set_src("signature", signature);
    }

    // Generate - never need to encrypt footer
    std::vector<uint8_t> empty_aes_key;
    bool bitgen_compress = false;
    uint64_t bop_data_size =
        bop.generate(bop_data, bitgen_compress, empty_aes_key, nullptr);
    CFG_ASSERT(bop_data_size > 0 && (bop_data_size % 32) == 0);

    // Package
    data.insert(data.end(), bop_data.begin(), bop_data.end());

    // Signing
    if (key != nullptr) {
      CFG_ASSERT(data.size() > signature.size());
      CFG_ASSERT(
          CFGOpenSSL::sign_message(&data[0], data.size() - signature.size(),
                                   &data[data.size() - signature.size()],
                                   signature.size(), key) == signature.size());
    }
  }
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

bool BitGen_GEMINI::generate(std::vector<uint8_t>& data, bool compress,
                             CFGCrypto_KEY*& key,
                             std::vector<uint8_t>& aes_key) {
  bool status = true;
  uint8_t iv[16];

  // cleanup
  data.clear();

  // FCB
  if (aes_key.size()) {
    CFGOpenSSL::generate_iv(iv, false);
  }
  BitGen_GEMINI_FCB_IMPL fcb;
  std::vector<uint8_t> fcb_data;
  fcb.set_src("parallel_chain_size", m_bitobj->fcb.width);
  fcb.set_src("bit_size_per_chain", m_bitobj->fcb.length);
  fcb.set_src("src_payload", m_bitobj->fcb.data);
  bool bitgen_compress = compress;
  uint64_t fcb_data_size = fcb.generate(fcb_data, bitgen_compress, aes_key, iv);
  uint64_t fcb_header_size = fcb.get_header_size();
  CFG_ASSERT(fcb_data_size > 0 && (fcb_data_size % 32) == 0);
  CFG_ASSERT(fcb_header_size > 0 && (fcb_header_size % 32) == 0);
  BitGen_GEMINI_BOP_HEADER_IMPL::package("fcb", fcb_data, bitgen_compress, key,
                                         aes_key.size(), fcb_header_size / 8);
  BitGen_GEMINI_BOP_FOOTER_IMPL::package(fcb_data, key, aes_key.size(), iv);
  data.insert(data.end(), fcb_data.begin(), fcb_data.end());

  // ICB
  uint8_t icb_iv[16];
  if (aes_key.size()) {
    CFGOpenSSL::generate_iv(iv, false);
    CFG_ASSERT(sizeof(icb_iv) == sizeof(iv));
    memcpy(icb_iv, iv, sizeof(icb_iv));
  }
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
    bitgen_compress = false;  // Do not support compression
    uint64_t icb_data_size =
        icb.generate(icb_packet_data, bitgen_compress, aes_key, iv);
    CFG_ASSERT(icb_data_size > 0 && (icb_data_size % 32) == 0);
    icb_data.insert(icb_data.end(), icb_packet_data.begin(),
                    icb_packet_data.end());
    index++;
    uint32_t iv_value = 0;
    memcpy(iv, (uint8_t*)(&iv_value), sizeof(iv_value));
    iv_value++;
    memcpy((uint8_t*)(&iv_value), iv, sizeof(iv_value));
  }
  BitGen_GEMINI_BOP_HEADER_IMPL::package("icb", icb_data, bitgen_compress, key,
                                         aes_key.size(), 4);
  BitGen_GEMINI_BOP_FOOTER_IMPL::package(icb_data, key, aes_key.size(), icb_iv);
  data.insert(data.end(), icb_data.begin(), icb_data.end());

  // PCB
  if (aes_key.size()) {
    CFGOpenSSL::generate_iv(iv, false);
  }
  std::vector<uint8_t> pcb_payload;
  for (uint32_t i = 0; i < (4096 * 3); i++) {
    CFG_append_u32(pcb_payload, i);
  }
  BitGen_GEMINI_PCB_PACKET_IMPL pcb;
  std::vector<uint8_t> pcb_data;
  pcb.set_src("src_payload", pcb_payload);
  bitgen_compress = compress;
  uint64_t pcb_data_size = pcb.generate(pcb_data, bitgen_compress, aes_key, iv);
  uint64_t pcb_header_size = fcb.get_header_size();
  CFG_ASSERT(pcb_data_size > 0 && (pcb_data_size % 32) == 0);
  CFG_ASSERT(pcb_header_size > 0 && (pcb_header_size % 32) == 0);
  BitGen_GEMINI_BOP_HEADER_IMPL::package("pcb", pcb_data, bitgen_compress, key,
                                         aes_key.size(), pcb_header_size / 8);
  BitGen_GEMINI_BOP_FOOTER_IMPL::package(pcb_data, key, aes_key.size(), iv);
  data.insert(data.end(), pcb_data.begin(), pcb_data.end());

  memset(iv, 0, sizeof(iv));
  return status;
}

void BitGen_GEMINI::parse(const std::string& input_filepath,
                          const std::string& output_filepath, bool detail) {
  std::vector<uint8_t> input_data;
  CFG_read_binary_file(input_filepath, input_data);
  std::ofstream file;
  file.open(output_filepath.c_str());
  CFG_ASSERT(file.is_open());
  CFG_ASSERT(input_data.size());

  uint64_t bit_index = 0;
  uint64_t total_bits = uint64_t(input_data.size() * 8);
  while (bit_index < total_bits) {
    BitGen_GEMINI_BOP_HEADER_IMPL bop_header;
    uint64_t bop_header_size = bop_header.parse(
        file, &input_data[0], total_bits, bit_index, false, "", detail);
    file.flush();
    CFG_ASSERT(bop_header_size > 0 && (bop_header_size % 32) == 0);
    std::vector<uint8_t> id = bop_header.get_rule_data("id");
    while (id.back() == 0) {
      id.pop_back();
    }
    const std::string image =
        get_bop_image(std::string((char*)(&id[0]), id.size()));
    uint64_t image_size = bop_header.get_rule_value("binary_length") +
                          bop_header.get_rule_value("xcb_header_size");
    uint64_t iv_length = bop_header.get_rule_value("iv_length");
    uint64_t public_key_length = bop_header.get_rule_value("public_key_length");
    uint64_t signature_length = bop_header.get_rule_value("signature_length");
    uint64_t compress = bop_header.get_rule_value("compression_algo");
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
          fcb.parse(file, &input_data[0], total_bits, bit_index, compress != 0,
                    space, detail);
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
        uint64_t icb_packet_size =
            icb.parse(file, &input_data[0], total_bits, bit_index,
                      compress != 0, space + "    ", detail);
        file.flush();
        CFG_ASSERT(icb_packet_size > 0 && (icb_packet_size % 32) == 0);
        icb_data_size += icb_packet_size;
      }
      CFG_ASSERT(icb_data_size == (image_size * 8));
    } else if (image == "pcb") {
      // PCB
      BitGen_GEMINI_PCB_PACKET_IMPL pcb;
      uint64_t pcb_data_size =
          pcb.parse(file, &input_data[0], total_bits, bit_index, compress != 0,
                    space, detail);
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
    if (iv_length != 0 || public_key_length != 0 || signature_length != 0) {
      if (public_key_length == 0) {
        CFG_ASSERT(signature_length == 0);
      } else {
        CFG_ASSERT(signature_length);
      }
      BitGen_GEMINI_BOP_FOOTER_IMPL bop_footer;
      if (iv_length) {
        bop_footer.set_rule_size("iv", iv_length * 8);
      }
      if (public_key_length) {
        while (public_key_length % 16) {
          public_key_length++;
        }
        bop_footer.set_rule_size("public_key", public_key_length * 8);
      }
      if (signature_length) {
        bop_footer.set_rule_size("signature", signature_length * 8);
      }
      uint64_t bop_footer_size = bop_footer.parse(
          file, &input_data[0], total_bits, bit_index, false, space, detail);
      file << "  ";
      for (uint8_t i = 0; i < 64; i++) {
        file << "*";
      }
      file << "\n";
      file.flush();
      CFG_ASSERT(bop_footer_size > 0 && (bop_footer_size % 32) == 0);
    }
  }
  CFG_ASSERT(bit_index == total_bits);

  // close file
  file.close();
}