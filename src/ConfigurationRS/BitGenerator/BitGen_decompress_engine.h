#ifndef BITGEN_DECOMPRESS_ENGINE_H
#define BITGEN_DECOMPRESS_ENGINE_H

#include "CFGCommonRS/CFGCommonRS.h"

enum BitGen_DECOMPRESS_ENGINE_STATUS {
  BitGen_DECOMPRESS_ENGINE_DONE_STATUS,
  BitGen_DECOMPRESS_ENGINE_GOOD_STATUS,
  BitGen_DECOMPRESS_ENGINE_NEED_INPUT_STATUS,
  BitGen_DECOMPRESS_ENGINE_ERROR_STATUS
};

enum BitGen_DECOMPRESS_ENGINE_STATE {
  BitGen_DECOMPRESS_ENGINE_GET_IDENTIFIER_STATE,
  BitGen_DECOMPRESS_ENGINE_GET_ORIGINAL_SIZE_STATE,
  BitGen_DECOMPRESS_ENGINE_GET_COMPRESS_SIZE_STATE,
  BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE,
  BitGen_DECOMPRESS_ENGINE_GET_VARIABLE_STATE,
  BitGen_DECOMPRESS_ENGINE_GET_REPEAT_STATE,
  BitGen_DECOMPRESS_ENGINE_OUTPUT_CMP_STATE,
  BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_STATE,
  BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_REPEAT_STATE,
  BitGen_DECOMPRESS_ENGINE_ERROR_STATE
};

enum BitGen_DECOMPRESS_ENGINE_CMP_TYPE {
  BitGen_DECOMPRESS_ENGINE_CMP_NONE_TYPE = 0,
  BitGen_DECOMPRESS_ENGINE_CMP_ZERO_TYPE = 1,
  BitGen_DECOMPRESS_ENGINE_CMP_ZERO_VAR_TYPE = 2,
  BitGen_DECOMPRESS_ENGINE_CMP_VAR_ZERO_TYPE = 3,
  BitGen_DECOMPRESS_ENGINE_CMP_HIGH_TYPE = 4,
  BitGen_DECOMPRESS_ENGINE_CMP_HIGH_VAR_TYPE = 5,
  BitGen_DECOMPRESS_ENGINE_CMP_VAR_HIGH_TYPE = 6,
  BitGen_DECOMPRESS_ENGINE_CMP_VAR_TYPE = 7
};

class BitGen_DECOMPRESS_ENGINE {
 public:
  BitGen_DECOMPRESS_ENGINE();
  ~BitGen_DECOMPRESS_ENGINE();
  void reset();
  BitGen_DECOMPRESS_ENGINE_STATUS process(const uint8_t* input,
                                          const size_t input_size,
                                          uint8_t* output,
                                          const size_t output_size,
                                          size_t& current_output_size);
  std::string get_coverage_info();

 protected:
  void init();
  void get_variable_u64(const uint8_t* input, const size_t input_size,
                        bool update);
  void get_identifier(const uint8_t* input, const size_t input_size);
  void get_information(const uint8_t* input, const size_t input_size);
  void get_flag(const uint8_t* input, const size_t input_size);
  void get_variable(const uint8_t* input, const size_t input_size);
  void get_repeat(const uint8_t* input, const size_t input_size);
  void output_cmp(uint8_t* output, const size_t output_size);
  void output_none(const uint8_t* input, const size_t input_size,
                   uint8_t* output, const size_t output_size);
  void output_none_repeat(uint8_t* output, const size_t output_size);

 private:
  BitGen_DECOMPRESS_ENGINE_STATE m_state = BitGen_DECOMPRESS_ENGINE_ERROR_STATE;
  BitGen_DECOMPRESS_ENGINE_STATUS m_status =
      BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
  size_t m_input_index = 0;         // track input index of each new input
  size_t m_output_index = 0;        // track output index of each engine call
  size_t m_total_input_index = 0;   // track total input index
  size_t m_total_output_index = 0;  // track total output index
  uint8_t m_none_data[2048];
  uint8_t m_identifier[8];
  size_t m_identifier_index = 0;
  size_t m_original_size = 0;
  size_t m_compress_size = 0;
  BitGen_DECOMPRESS_ENGINE_CMP_TYPE m_flag =
      BitGen_DECOMPRESS_ENGINE_CMP_NONE_TYPE;
  bool m_flag_repeat = 0;
  size_t m_flag_index = 0;
  size_t m_flag_size = 0;
  size_t m_repeat_index = 0;
  size_t m_repeat_size = 0;
  uint64_t m_variable_u64 = 0;
  size_t m_variable_u64_index = 0;
  bool m_variable_u64_done = false;
  bool m_need_more_input = false;
  bool m_need_more_output = false;
  uint8_t m_compress_variable = 0;
  uint8_t m_variable = 0;
  uint8_t m_track_cmp = 0;
  bool m_output_variable = false;
  bool m_done = false;
  uint32_t m_coverage = 0;
  std::vector<std::string> m_error_msgs;
};

#endif
