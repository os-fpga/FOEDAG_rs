#include "BitGen_decompress_engine.h"

#define BitGen_DECOMPRESS_ENGINE_VERSION (0)
#define ENABLE_DEBUG (0)

BitGen_DECOMPRESS_ENGINE::BitGen_DECOMPRESS_ENGINE() {
  m_state = BitGen_DECOMPRESS_ENGINE_ERROR_STATE;
  m_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
}

BitGen_DECOMPRESS_ENGINE::~BitGen_DECOMPRESS_ENGINE() {
  memset(m_none_data, 0, sizeof(m_none_data));
}

void BitGen_DECOMPRESS_ENGINE::reset() {
  // This must be called everytime of new decompression
  // Reset all varaiable
  m_state = BitGen_DECOMPRESS_ENGINE_GET_IDENTIFIER_STATE;
  m_status = BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
  m_input_index = 0;
  m_output_index = 0;
  m_total_input_index = 0;
  m_total_output_index = 0;
  m_identifier_index = 0;
  m_original_size = 0;
  m_compress_size = 0;
  m_flag = BitGen_DECOMPRESS_ENGINE_CMP_NONE_TYPE;
  m_flag_repeat = 0;
  m_flag_index = 0;
  m_flag_size = 0;
  m_repeat_index = 0;
  m_repeat_size = 0;
  m_variable_u64 = 0;
  m_variable_u64_index = 0;
  m_variable_u64_done = false;
  m_need_more_input = false;
  m_need_more_output = false;
  m_compress_variable = 0;
  m_variable = 0;
  m_track_cmp = 0;
  m_output_variable = false;
  m_done = false;
  m_coverage = 0;
  m_error_msgs.clear();
}

BitGen_DECOMPRESS_ENGINE_STATUS BitGen_DECOMPRESS_ENGINE::process(
    const uint8_t* input, const size_t input_size, uint8_t* output,
    const size_t output_size, size_t& current_output_size) {
  CFG_ASSERT(input != NULL);
  CFG_ASSERT(input_size > 0);
  CFG_ASSERT(output != NULL);
  CFG_ASSERT(output_size > 0);
  ;
  // Once it is done, caller must reset engine
  if (m_status == BitGen_DECOMPRESS_ENGINE_DONE_STATUS) {
    m_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
    m_error_msgs.push_back("Engine is called after status is DONE.");
  } else if (m_status == BitGen_DECOMPRESS_ENGINE_NEED_INPUT_STATUS) {
    // If last status is need input and caller call engine again
    // mean caller already prepare new set of input
    // hence we reset the input index
    m_input_index = 0;
    m_need_more_input = false;
    m_status = BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
  } else if (m_status == BitGen_DECOMPRESS_ENGINE_GOOD_STATUS &&
             m_need_more_input) {
    m_status = BitGen_DECOMPRESS_ENGINE_NEED_INPUT_STATUS;
  }
  // Every engine call, output will be reset
  m_need_more_output = false;
  m_output_index = 0;
  // Once error had been set, caller must reset engine
  while (m_status == BitGen_DECOMPRESS_ENGINE_GOOD_STATUS) {
    if (m_state != BitGen_DECOMPRESS_ENGINE_GET_IDENTIFIER_STATE) {
      if (m_state != BitGen_DECOMPRESS_ENGINE_GET_ORIGINAL_SIZE_STATE) {
#if ENABLE_DEBUG
        if (!(m_total_output_index < m_original_size)) {
          printf("  Error: %d vs %d\n", (uint32_t)(m_total_output_index),
                 (uint32_t)(m_original_size));
        }
#endif
        CFG_ASSERT(m_total_output_index < m_original_size);
        if (m_state != BitGen_DECOMPRESS_ENGINE_GET_COMPRESS_SIZE_STATE) {
#if ENABLE_DEBUG
          if (!(m_total_input_index <= m_compress_size)) {
            printf("  Error: %d vs %d\n", (uint32_t)(m_total_input_index),
                   (uint32_t)(m_compress_size));
          }
#endif
          CFG_ASSERT(m_total_input_index <= m_compress_size);
        }
      }
    }
#if ENABLE_DEBUG
    printf("State: %d\n", m_state);
#endif
    switch (m_state) {
      case BitGen_DECOMPRESS_ENGINE_GET_IDENTIFIER_STATE:
        get_identifier(input, input_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_GET_ORIGINAL_SIZE_STATE:
      case BitGen_DECOMPRESS_ENGINE_GET_COMPRESS_SIZE_STATE:
        get_information(input, input_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE:
        get_flag(input, input_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_GET_VARIABLE_STATE:
        get_variable(input, input_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_GET_REPEAT_STATE:
        get_repeat(input, input_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_OUTPUT_CMP_STATE:
        output_cmp(output, output_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_STATE:
        output_none(input, input_size, output, output_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_REPEAT_STATE:
        output_none_repeat(output, output_size);
        break;
      case BitGen_DECOMPRESS_ENGINE_ERROR_STATE:
      default:
        m_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
        break;
    }
#if ENABLE_DEBUG
    printf("  Next State: %d\n", m_state);
#endif
    if (m_status == BitGen_DECOMPRESS_ENGINE_ERROR_STATUS) {
      break;
    } else if (m_done) {
      if (m_total_input_index == m_compress_size) {
        m_status = BitGen_DECOMPRESS_ENGINE_DONE_STATUS;
      } else {
        m_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
        m_error_msgs.push_back(
            "Decompressed data input size does not match with Header "
            "Compressed size");
      }
      break;
    } else if (m_need_more_input) {
      if (m_output_index == 0) {
        m_status = BitGen_DECOMPRESS_ENGINE_NEED_INPUT_STATUS;
      } else {
        m_status = BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
      }
      break;
    } else if (m_need_more_output) {
      CFG_ASSERT(m_output_index == output_size);
      break;
    }
  }
  // Regardless, just copy but only valid for
  // BitGen_DECOMPRESS_ENGINE_GOOD_STATUS and
  // BitGen_DECOMPRESS_ENGINE_DONE_STATUS
  current_output_size = m_output_index;
#if ENABLE_DEBUG
  printf("  Status: %d, OutputSize: %d\n", m_status,
         (uint32_t)(m_output_index));
#endif
  return m_status;
}

void BitGen_DECOMPRESS_ENGINE::get_variable_u64(const uint8_t* input,
                                                const size_t input_size,
                                                bool update) {
  CFG_ASSERT(!m_variable_u64_done);
  CFG_ASSERT(!m_need_more_input);
  if (m_input_index < input_size) {
    while (1) {
      // maximum is uint64_t (10 bytes which including next bit)
      CFG_ASSERT(m_variable_u64_index < 10);
      if (m_input_index == input_size) {
        // Not enough memory
        m_need_more_input = true;
        break;
      }
      m_variable_u64_done = (input[m_input_index] & 0x80) == 0;
      m_variable_u64 |= (uint64_t)((uint64_t)(input[m_input_index] & 0x7F)
                                   << (m_variable_u64_index * 7));
      m_variable_u64_index++;
      m_input_index++;
      if (update) {
        m_total_input_index++;
      }
      if (m_variable_u64_done) {
        break;
      }
    }
  } else {
    CFG_ASSERT(m_input_index == input_size);
    m_need_more_input = true;
  }
}

void BitGen_DECOMPRESS_ENGINE::get_identifier(const uint8_t* input,
                                              const size_t input_size) {
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_GET_IDENTIFIER_STATE);
  CFG_ASSERT(!m_need_more_input);
  CFG_ASSERT(m_input_index < input_size);
  CFG_ASSERT(m_identifier_index < (size_t)(sizeof(m_identifier)));
  while (m_identifier_index < (size_t)(sizeof(m_identifier)) &&
         m_input_index < input_size) {
    CFG_ASSERT(m_input_index < input_size);
    m_identifier[m_identifier_index++] = input[m_input_index++];
  }
  if (m_identifier_index == (size_t)(sizeof(m_identifier))) {
    if (memcmp(m_identifier, "CFG_CMP", 7) == 0 &&
        m_identifier[7] == BitGen_DECOMPRESS_ENGINE_VERSION) {
      // Good
      m_state = BitGen_DECOMPRESS_ENGINE_GET_ORIGINAL_SIZE_STATE;
    } else {
      m_state = BitGen_DECOMPRESS_ENGINE_ERROR_STATE;
      m_error_msgs.push_back(
          CFG_print("Invalid Identifier: %s",
                    CFG_convert_bytes_to_hex_string(m_identifier, 8).c_str()));
    }
  } else {
    m_need_more_input = true;
  }
}

void BitGen_DECOMPRESS_ENGINE::get_information(const uint8_t* input,
                                               const size_t input_size) {
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_GET_ORIGINAL_SIZE_STATE ||
             m_state == BitGen_DECOMPRESS_ENGINE_GET_COMPRESS_SIZE_STATE);
  get_variable_u64(input, input_size, false);
  if (m_variable_u64_done) {
    if (m_state == BitGen_DECOMPRESS_ENGINE_GET_ORIGINAL_SIZE_STATE) {
      m_state = BitGen_DECOMPRESS_ENGINE_GET_COMPRESS_SIZE_STATE;
      m_original_size = (size_t)(m_variable_u64);
#if ENABLE_DEBUG
      printf("Original Size: %d\n", (uint32_t)(m_original_size));
#endif
    } else {
      m_state = BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE;
      m_compress_size = (size_t)(m_variable_u64);
#if ENABLE_DEBUG
      printf("Compressed Size: %d\n", (uint32_t)(m_compress_size));
#endif
    }
    m_variable_u64_done = false;
    m_variable_u64_index = 0;
    m_variable_u64 = 0;
  }
}

void BitGen_DECOMPRESS_ENGINE::get_flag(const uint8_t* input,
                                        const size_t input_size) {
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE);
  get_variable_u64(input, input_size, true);
  if (m_variable_u64_done) {
    m_flag = (BitGen_DECOMPRESS_ENGINE_CMP_TYPE)(m_variable_u64 & 0x7);
    m_flag_repeat = (m_variable_u64 & 0x8) != 0;
    m_flag_size = (size_t)(m_variable_u64 >> 4);
    // Reset compression
    m_flag_index = 0;
    m_repeat_index = 0;
    m_compress_variable = 0;
    m_output_variable = false;
    m_track_cmp = (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_ZERO_TYPE ||
                   m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_TYPE ||
                   m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_TYPE)
                      ? 1
                      : 0;
    m_coverage |= (1 << (m_variable_u64 & 0xF));
    // Reset variable u64
    m_variable_u64_done = false;
    m_variable_u64_index = 0;
    m_variable_u64 = 0;
    CFG_ASSERT(m_flag_size);
    if (m_flag_repeat && (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_ZERO_TYPE ||
                          m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_TYPE ||
                          m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_TYPE)) {
      m_state = BitGen_DECOMPRESS_ENGINE_ERROR_STATE;
      m_error_msgs.push_back(
          CFG_print("Repeat feature should not support CMP Type: %d", m_flag));
    } else if (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_ZERO_VAR_TYPE ||
               m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_ZERO_TYPE ||
               m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_VAR_TYPE ||
               m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_HIGH_TYPE ||
               m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_TYPE) {
      m_state = BitGen_DECOMPRESS_ENGINE_GET_VARIABLE_STATE;
    } else if (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_ZERO_TYPE ||
               m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_TYPE) {
      m_state = BitGen_DECOMPRESS_ENGINE_OUTPUT_CMP_STATE;
    } else {
      m_state = BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_STATE;
    }
    if (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_TYPE ||
        m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_VAR_TYPE ||
        m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_HIGH_TYPE) {
      m_compress_variable = 0xFF;
    }
    if (m_flag_repeat && m_flag == BitGen_DECOMPRESS_ENGINE_CMP_NONE_TYPE) {
      if (m_flag_size > (size_t)(sizeof(m_none_data))) {
        m_state = BitGen_DECOMPRESS_ENGINE_ERROR_STATE;
        m_error_msgs.push_back(CFG_print(
            "Do not have enough buffer to store REPEATED-NONE with size %ld",
            m_flag_size));
      }
    }
  }
}

void BitGen_DECOMPRESS_ENGINE::get_variable(const uint8_t* input,
                                            const size_t input_size) {
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_GET_VARIABLE_STATE);
  CFG_ASSERT(!m_need_more_input);
  if (m_input_index < input_size) {
    m_variable = input[m_input_index++];
    m_total_input_index++;
    if (m_flag_repeat) {
      m_state = BitGen_DECOMPRESS_ENGINE_GET_REPEAT_STATE;
    } else {
      m_state = BitGen_DECOMPRESS_ENGINE_OUTPUT_CMP_STATE;
    }
    if (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_TYPE) {
      m_compress_variable = m_variable;
    }
  } else {
    CFG_ASSERT(m_input_index == input_size);
    m_need_more_input = true;
  }
}

void BitGen_DECOMPRESS_ENGINE::get_repeat(const uint8_t* input,
                                          const size_t input_size) {
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_GET_REPEAT_STATE);
  get_variable_u64(input, input_size, true);
  if (m_variable_u64_done) {
    m_repeat_size = m_variable_u64;
    m_variable_u64_done = false;
    m_variable_u64_index = 0;
    m_variable_u64 = 0;
    CFG_ASSERT(m_repeat_size);
    if (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_NONE_TYPE) {
      m_state = BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_REPEAT_STATE;
    } else {
      m_state = BitGen_DECOMPRESS_ENGINE_OUTPUT_CMP_STATE;
    }
  }
}

void BitGen_DECOMPRESS_ENGINE::output_cmp(uint8_t* output,
                                          const size_t output_size) {
#if ENABLE_DEBUG
  printf("  Flag: %d, Repeat: %d, Input: %d vs %d, Ouput: %d vs %d\n", m_flag,
         (uint32_t)(m_repeat_size), (uint32_t)(m_total_input_index),
         (uint32_t)(m_compress_size), (uint32_t)(m_total_output_index),
         (uint32_t)(m_original_size));
#endif
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_OUTPUT_CMP_STATE);
  CFG_ASSERT(m_output_index < output_size);
  CFG_ASSERT(m_total_input_index <= m_compress_size);
  CFG_ASSERT(m_total_output_index < m_original_size);
  while (m_output_index < output_size) {
    CFG_ASSERT(m_total_output_index < m_original_size);
    if (!m_output_variable &&
        (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_ZERO_TYPE ||
         m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_HIGH_TYPE)) {
      m_output_variable = true;
      output[m_output_index++] = m_variable;
      m_total_output_index++;
      m_track_cmp |= 1;
    } else if (m_flag_index < m_flag_size) {
      output[m_output_index++] = m_compress_variable;
      m_total_output_index++;
      m_flag_index++;
      m_track_cmp |= (m_flag_index == m_flag_size ? 2 : 0);
    } else if (!m_output_variable &&
               (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_ZERO_VAR_TYPE ||
                m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_VAR_TYPE)) {
      m_output_variable = true;
      output[m_output_index++] = m_variable;
      m_total_output_index++;
      m_track_cmp |= 1;
    }
    // Detect done
    if (m_track_cmp == 3) {
      // Done but do we have repeat?!?
      m_track_cmp = (m_flag == BitGen_DECOMPRESS_ENGINE_CMP_ZERO_TYPE ||
                     m_flag == BitGen_DECOMPRESS_ENGINE_CMP_HIGH_TYPE ||
                     m_flag == BitGen_DECOMPRESS_ENGINE_CMP_VAR_TYPE)
                        ? 1
                        : 0;
      if (m_flag_repeat) {
        m_output_variable = false;
        m_flag_index = 0;
        m_repeat_index++;
        if (m_repeat_index == (m_repeat_size + 1)) {
          m_state = BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE;
          m_done = m_total_output_index == m_original_size;
#if ENABLE_DEBUG
          printf("  Done: %d (%d vs %d)\n", m_done,
                 (uint32_t)(m_total_output_index), (uint32_t)(m_original_size));
#endif
          break;
        } else {
          CFG_ASSERT(m_repeat_index <= m_repeat_size);
        }
      } else {
        m_state = BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE;
        m_done = m_total_output_index == m_original_size;
#if ENABLE_DEBUG
        printf("  Done: %d (%d vs %d)\n", m_done,
               (uint32_t)(m_total_output_index), (uint32_t)(m_original_size));
#endif
        break;
      }
    }
  }
  m_need_more_output = (m_output_index == output_size);
}

void BitGen_DECOMPRESS_ENGINE::output_none(const uint8_t* input,
                                           const size_t input_size,
                                           uint8_t* output,
                                           const size_t output_size) {
#if ENABLE_DEBUG
  printf("  (%d vs %d), (%d vs %d), (%d vs %d)\n", (uint32_t)(m_flag_index),
         (uint32_t)(m_flag_size), (uint32_t)(m_input_index),
         (uint32_t)(input_size), (uint32_t)(m_output_index),
         (uint32_t)(output_size));
#endif
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_STATE);
  CFG_ASSERT(m_total_input_index < m_compress_size);
  CFG_ASSERT(m_total_output_index < m_original_size);
  while (m_flag_index < m_flag_size && m_input_index < input_size &&
         m_output_index < output_size) {
    CFG_ASSERT(m_total_output_index < m_original_size);
    CFG_ASSERT(m_total_input_index < m_compress_size);
    output[m_output_index++] = input[m_input_index];
    if (m_flag_repeat) {
      m_none_data[m_flag_index] = input[m_input_index];
    }
    m_flag_index++;
    m_input_index++;
    m_total_output_index++;
    m_total_input_index++;
  }
  if (m_flag_index == m_flag_size) {
    m_flag_index = 0;
    m_done = m_total_output_index == m_original_size;
    if (m_done) {
      CFG_ASSERT(!m_flag_repeat);
      m_state = BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE;
    } else if (m_flag_repeat) {
      m_state = BitGen_DECOMPRESS_ENGINE_GET_REPEAT_STATE;
    } else {
      m_state = BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE;
    }
  }
  m_need_more_input = (m_input_index == input_size);
  m_need_more_output = (m_output_index == output_size);
}

void BitGen_DECOMPRESS_ENGINE::output_none_repeat(uint8_t* output,
                                                  const size_t output_size) {
#if ENABLE_DEBUG
  printf("  RepeatIndex: %d, Repeat: %d, Size: %d\n",
         (uint32_t)(m_repeat_index), (uint32_t)(m_repeat_size),
         (uint32_t)(m_flag_size));
#endif
  CFG_ASSERT(m_state == BitGen_DECOMPRESS_ENGINE_OUTPUT_NONE_REPEAT_STATE);
  CFG_ASSERT(m_flag_size);
  CFG_ASSERT(m_repeat_size);
  CFG_ASSERT(m_flag_index < m_flag_size);
  CFG_ASSERT(m_repeat_index < m_repeat_size);
  CFG_ASSERT(m_flag_size <= (size_t)(sizeof(m_none_data)));
  CFG_ASSERT(m_total_input_index <= m_compress_size);
  CFG_ASSERT(m_total_output_index < m_original_size);
  while (m_repeat_index < m_repeat_size) {
    while (m_output_index < output_size && m_flag_index < m_flag_size) {
      CFG_ASSERT(m_total_output_index < m_original_size);
      output[m_output_index++] = m_none_data[m_flag_index++];
      m_total_output_index++;
    }
    CFG_ASSERT(m_output_index <= output_size);
    if (m_flag_index == m_flag_size) {
      m_flag_index = 0;
      m_repeat_index++;
    }
    if (m_output_index == output_size) {
      break;
    }
  }
  if (m_repeat_index == m_repeat_size) {
    m_done = m_total_output_index == m_original_size;
    m_state = BitGen_DECOMPRESS_ENGINE_GET_FLAG_STATE;
  }
  m_need_more_output = (m_output_index == output_size);
#if ENABLE_DEBUG
  printf("  Repeat loop: %d vs %d\n", (uint32_t)(m_repeat_index),
         (uint32_t)(m_repeat_size));
#endif
}

std::string BitGen_DECOMPRESS_ENGINE::get_coverage_info() {
  std::string info = "";
  if (m_status == BitGen_DECOMPRESS_ENGINE_DONE_STATUS) {
    for (uint32_t i = 0; i < 16; i++) {
      if ((m_coverage & (1 << i)) == 0) {
        std::string type = "";
        switch (i) {
          case 0:
            type = "NONE";
            break;
          case 1:
            type = "ZERO";
            break;
          case 2:
            type = "ZERO-VAR";
            break;
          case 3:
            type = "VAR-ZERO";
            break;
          case 4:
            type = "HIGH";
            break;
          case 5:
            type = "HIGH-VAR";
            break;
          case 6:
            type = "VAR-HIGH";
            break;
          case 7:
            type = "VAR";
            break;
          case 8:
            type = "REPEATED-NONE";
            break;
          case 10:
            type = "REPEATED-ZERO-VAR";
            break;
          case 11:
            type = "REPEATED-VAR-ZERO";
            break;
          case 13:
            type = "REPEATED-HIGH-VAR";
            break;
          case 14:
            type = "REPEATED-VAR-HIGH";
            break;
          default:
            break;
        }
        if (type.size()) {
          if (info.size()) {
            info = CFG_print("%s, %s", info.c_str(), type.c_str());
          } else {
            info = type;
          }
        }
      }
    }
    if (info.empty()) {
      info = "All types are covered";
    } else {
      info = CFG_print("The bitstream does not cover type: %s", info.c_str());
    }
  } else {
    info = "Decompress Engine status is not done. No coverage info";
  }
  return info;
}