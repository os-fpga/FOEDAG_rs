#ifndef BITGEN_ANALYZER_H
#define BITGEN_ANALYZER_H

#include "BitGen_decompress_engine.h"
#include "BitGen_packer.h"
#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGCrypto/CFGCrypto_key.h"

struct BitGen_ANALYZER_BOP_HEADER {
  void reset() {
    identifier = "";
    version = 0;
    size = 0;
    tool = "";
    opn = "";
    jtag_id = 0;
    jtag_mask = 0;
    chipid = "";
    checksum = "";
    compression = "";
    integrity = "";
    encryption = "";
    authentication = "";
    action_version = 0;
    action_count = 0;
    is_last_bop_block = false;
    end_size = 0;
    crc32 = 0;
    memset(iv, 0, sizeof(iv));
  }
  std::string identifier = "";
  uint32_t version = 0;
  size_t size = 0;
  std::string tool = "";
  std::string opn = "";
  uint32_t jtag_id = 0;
  uint32_t jtag_mask = 0;
  std::string chipid = "";
  std::string checksum = "";
  std::string compression = "";
  std::string integrity = "";
  std::string encryption = "";
  std::string authentication = "";
  uint32_t action_version = 0;
  uint32_t action_count = 0;
  bool is_last_bop_block = false;
  size_t end_size = 0;
  uint32_t crc32 = 0;
  uint8_t iv[16] = {0};
};

struct BitGen_ANALYZER_STATUS {
  void reset() {
    status = true;
    action_status = true;
    checksum_status = true;
    compression_status = true;
    integrity_status = true;
    encryption_status = true;
    authentication_status = true;
    decompression_status = BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
  }
  bool status = true;
  bool action_status = true;
  bool checksum_status = true;
  bool compression_status = true;
  bool integrity_status = true;
  bool encryption_status = true;
  bool authentication_status = true;
  BitGen_DECOMPRESS_ENGINE_STATUS decompression_status =
      BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
};

struct BitGen_ANALYZER_ACTION {
  ~BitGen_ANALYZER_ACTION() { memset(iv, 0, sizeof(iv)); }
  uint16_t cmd = 0;
  bool has_checksum = false;
  bool compression = false;
  bool has_iv = false;
  bool has_original_payload_size = false;
  uint16_t size = 0;
  uint16_t field_size = 0;
  uint16_t iv_size = 0;
  uint8_t iv[16] = {0};
  uint32_t payload_size = 0;
  uint32_t original_payload_size = 0;
  uint32_t checksum_value = 0;
  // tracker
  uint16_t field_tracker = 0;
  uint16_t iv_tracker = 0;
};

struct BitGen_ANALYZER_INTEGRITY {
  void reset() {
    size = 0;
    remaining_size = 0;
    addr = nullptr;
  }
  size_t size = 0;
  size_t remaining_size = 0;
  uint8_t* addr = nullptr;
};

class BitGen_ANALYZER {
 public:
  static void combine_bitstreams(const std::string& input1_filepath,
                                 const std::string& input2_filepath,
                                 const std::string& output_filepath);
  static bool combine_bitstreams(std::vector<uint8_t>& input_data,
                                 std::vector<uint8_t>& output_data,
                                 bool print_msg = true,
                                 bool clear_input_if_success = true);
  static void update_bitstream_end_size(std::vector<uint8_t>& data,
                                        std::string& error_msg);
  static std::vector<size_t> parse(const std::vector<uint8_t>& data,
                                   bool check_end_size, bool check_crc,
                                   std::string& error_msg, bool print_msg);
  static void parse_debug(const std::string& input_filepath,
                          const std::string& output_filepath,
                          std::vector<uint8_t>& aes_key);

 protected:
  template <typename T>
  static void get_data(const uint8_t* data, T& value);
  static uint32_t get_u32(const uint8_t* data);
  static uint64_t get_u64(const uint8_t* data);

 protected:
  BitGen_ANALYZER(const std::string& filepath, std::ofstream* file,
                  std::vector<uint8_t>* aes_key);
  ~BitGen_ANALYZER();
  void update_iv(uint8_t* iv);
  std::string print_repeat_word_line(std::string word, uint32_t repeat);
  bool parse_bop(const uint8_t* data, const size_t size, const uint32_t index,
                 const size_t offset);
  void parse_bop_header(std::string space, uint8_t (&unobscured_data)[16]);
  void parse_action(const uint32_t* data, size_t size, uint32_t& action_index,
                    std::vector<std::string>& action_msgs,
                    std::vector<BitGen_ANALYZER_ACTION*>& actions);
  void print_action_block(std::string space, const uint8_t* data,
                          size_t action_addr,
                          std::vector<std::string>& action_msgs);
  void authenticate(std::string space);
  void challenge(std::string space, std::vector<std::string>& msgs);
  void print_header(std::string space, uint8_t (&unobscured_data)[16],
                    std::vector<std::string>& action_msgs,
                    std::vector<std::string>& challenge_msgs);
  size_t get_next_block(std::string space, std::string block_name);
  void print_hash_block(std::string space, const uint8_t* data);
  void parse_payload(std::string space, const uint8_t* data, size_t size,
                     size_t payload_addr,
                     bool action_force_turn_off_compression, bool action_iv,
                     uint8_t* iv, bool is_last_payload_block,
                     std::ofstream& binfile);
  void post_warning(const std::string& space, const std::string& msg,
                    bool new_line = true);
  void post_error(const std::string& space, const std::string& msg,
                  bool new_line = true);
  void push_error(std::vector<std::string>& msgs, const std::string& msg,
                  const std::string space = " ",
                  const std::string new_line = "");

 private:
  std::string m_filepath = "";
  std::ofstream* m_file = nullptr;
  std::vector<uint8_t>* m_aes_key = nullptr;
  const uint8_t* m_current_bop_data = nullptr;
  size_t m_current_bop_data_index = 0;
  size_t m_current_bop_size = 0;
  uint32_t m_current_bop_index = 0;
  size_t m_current_bop_offset = 0;
  BitGen_ANALYZER_STATUS m_status;
  BitGen_ANALYZER_BOP_HEADER m_header;
  BitGen_ANALYZER_INTEGRITY m_integrity;
  BitGen_DECOMPRESS_ENGINE m_decompress_engine;
  uint8_t m_decompressed_data[BitGen_BITSTREAM_BLOCK_SIZE] = {0};
  size_t m_decompressed_data_offset = 0;
};

#endif