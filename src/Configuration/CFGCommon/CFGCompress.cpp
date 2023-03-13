#include <CFGCompress.h>
#include <CFGHelper.h>

#define CFG_CMP_MIN_REPEAT_LENGTH_SEARCH (3)
#define CFG_CMP_MAX_REPEAT_LENGTH_SEARCH (50)
#define CFG_CMP_MAX_REPEAT_SAME_PATTERN (100)
#define CFG_CMP_NONE (0)
#define CFG_CMP_ZERO (1)
#define CFG_CMP_ZERO_VAR (2)
#define CFG_CMP_VAR_ZERO (3)
#define CFG_CMP_HIGH (4)
#define CFG_CMP_HIGH_VAR (5)
#define CFG_CMP_VAR_HIGH (6)
#define CFG_CMP_VAR (7)
#define CFG_CMP_INVALID (8)

void CFG_COMPRESS::analyze_repeat(const uint8_t* input, const size_t input_size,
                                  size_t index, size_t& length, size_t& repeat,
                                  const uint8_t debug) {
  length = 0;
  repeat = 0;
  size_t total_length = input_size - index;
  if (total_length < (2 * CFG_CMP_MIN_REPEAT_LENGTH_SEARCH)) {
    return;
  }
  size_t temp_length = CFG_CMP_MIN_REPEAT_LENGTH_SEARCH;
  uint8_t repeated_check_seq = 0;
  uint8_t repeated_byte = input[index];
  size_t repeated_check_index = index;
  size_t repeated_check_size = 0;
  while (temp_length <= (size_t)(total_length / 2)) {
    if (length == 0 && temp_length > CFG_CMP_MAX_REPEAT_LENGTH_SEARCH) {
      break;
    }
    size_t chunk0_start = index;
    CFG_ASSERT(chunk0_start < input_size);
    CFG_ASSERT((chunk0_start + temp_length) < input_size);
    size_t chunk1_start = chunk0_start + temp_length;
    CFG_ASSERT(chunk1_start < input_size);
    CFG_ASSERT((chunk1_start + temp_length) <= input_size);
    if (repeated_check_seq == 0) {
      repeated_check_seq = 1;
      for (uint32_t i = 0; i < (2 * CFG_CMP_MIN_REPEAT_LENGTH_SEARCH);
           i++, repeated_check_index++, repeated_check_size++) {
        if (input[repeated_check_index] != repeated_byte) {
          repeated_check_seq = 2;
          break;
        }
      }
    } else if (repeated_check_seq == 1) {
      if (input[repeated_check_index++] == repeated_byte &&
          input[repeated_check_index++] == repeated_byte) {
        repeated_check_size += 2;
        if (repeated_check_size >= CFG_CMP_MAX_REPEAT_SAME_PATTERN) {
          length = 0;
          repeat = 0;
          break;
        }
      } else {
        repeated_check_seq = 2;
      }
    }
    if (memcmp(&input[chunk0_start], &input[chunk1_start], temp_length) == 0) {
      length = temp_length;
    } else if (length) {
      break;
    }
    temp_length++;
  }
  if (length) {
    // We found repeated pattern
    size_t chunk0_start = index;
    size_t chunk1_start = chunk0_start + length;
    size_t chunk1_end_p1 = chunk1_start + length;
    while (chunk1_end_p1 <= input_size) {
      if (memcmp(&input[chunk0_start], &input[chunk1_start], length) == 0) {
        chunk1_start += length;
        chunk1_end_p1 += length;
        repeat++;
      } else {
        break;
      }
    }
    CFG_ASSERT(repeat);
    if (debug) {
      printf("Found repated %ld byte(s) of data for %ld time(s) at index %ld\n",
             length, repeat, index);
    }
  }
}

size_t CFG_COMPRESS::analyze_chunk_pattern(const uint8_t* input,
                                           const size_t input_size,
                                           size_t index, size_t length) {
  CFG_ASSERT(length >= CFG_CMP_MIN_REPEAT_LENGTH_SEARCH);
  size_t zero_length = 0;
  size_t high_length = 0;
  for (size_t i = 0; i < length; i++, index++) {
    CFG_ASSERT(index < input_size);
    if (input[index] == 0) {
      zero_length++;
    } else if (input[index] == 0xFF) {
      high_length++;
    }
  }
  if (zero_length == length) {
    return CFG_CMP_ZERO;
  } else if (high_length == length) {
    return CFG_CMP_HIGH;
  } else if (zero_length == (length - 1)) {
    if (input[index] != 0) {
      // Remember this is pointing to next chunk first byte, not the original
      // chunk
      return CFG_CMP_VAR_ZERO;
    } else if (input[index - 1] != 0) {
      // Remember this is pointing to original chunk last byte
      return CFG_CMP_ZERO_VAR;
    } else {
      return CFG_CMP_NONE;
    }
  } else if (high_length == (length - 1)) {
    if (input[index] != 0xFF) {
      // Remember this is pointing to next chunk first byte, not the original
      // chunk
      return CFG_CMP_VAR_HIGH;
    } else if (input[index - 1] != 0xFF) {
      // Remember this is pointing to original chunk last byte
      return CFG_CMP_HIGH_VAR;
    } else {
      return CFG_CMP_NONE;
    }
  } else {
    return CFG_CMP_NONE;
  }
}

void CFG_COMPRESS::compress_data(std::vector<uint8_t>& output,
                                 uint8_t compress_byte, size_t length,
                                 int followup_byte, const bool debug,
                                 std::string space) {
  CFG_ASSERT(length > 0);
  if (debug) {
    if (followup_byte == -1) {
      printf("%sCompress %ld data of 0x%02X\n", space.c_str(), length,
             compress_byte);
    } else {
      printf("%sCompress %ld data of 0x%02X - followed by 0x%02X\n",
             space.c_str(), length, compress_byte, followup_byte);
    }
  }
  size_t pattern = CFG_CMP_INVALID;
  if (compress_byte == 0) {
    pattern = followup_byte == -1 ? CFG_CMP_ZERO : CFG_CMP_ZERO_VAR;
  } else if (compress_byte == 0xFF) {
    pattern = followup_byte == -1 ? CFG_CMP_HIGH : CFG_CMP_HIGH_VAR;
  } else {
    CFG_ASSERT(compress_byte > 0 && compress_byte < 0xFF);
    CFG_ASSERT(followup_byte == -1);
    pattern = CFG_CMP_VAR;
  }
  uint64_t flag = ((uint64_t)(length) << 4) | pattern;
  CFG_write_variable_u64(output, flag);
  if (pattern == CFG_CMP_VAR) {
    output.push_back(compress_byte);
  } else if (followup_byte != -1) {
    CFG_ASSERT(followup_byte >= 0 && followup_byte <= 0xFF);
    output.push_back((uint8_t)(followup_byte));
  }
}

void CFG_COMPRESS::compress_repeat_chunk(const uint8_t* input,
                                         const size_t input_size,
                                         std::vector<uint8_t>& output,
                                         size_t index, size_t pattern,
                                         size_t length, size_t repeat) {
  CFG_ASSERT(length >= CFG_CMP_MIN_REPEAT_LENGTH_SEARCH);
  CFG_ASSERT(repeat > 0);
  CFG_ASSERT(index < input_size);
  CFG_ASSERT((index + length) < input_size);
  uint64_t flag = 0;
  if (pattern == CFG_CMP_NONE) {
    flag = ((uint64_t)(length) << 4) | 0x08 | (uint64_t)(pattern);
  } else {
    flag = ((uint64_t)(length - 1) << 4) | 0x08 | (uint64_t)(pattern);
  }
  CFG_write_variable_u64(output, flag);
  if (pattern == CFG_CMP_VAR_ZERO || pattern == CFG_CMP_VAR_HIGH) {
    output.push_back(input[index]);
  } else if (pattern == CFG_CMP_ZERO_VAR || pattern == CFG_CMP_HIGH_VAR) {
    output.push_back(input[index + length - 1]);
  } else {
    CFG_ASSERT(pattern == CFG_CMP_NONE);
    for (size_t i = 0; i < length; i++, index++) {
      output.push_back(input[index]);
    }
  }
  CFG_write_variable_u64(output, (uint64_t)(repeat));
}

void CFG_COMPRESS::pack_data(const uint8_t* input, const size_t input_size,
                             std::vector<uint8_t>& output, size_t index,
                             size_t length, const bool debug) {
  CFG_ASSERT(length);
  if (debug) {
    printf("Pack %ld byte(s) data at index 0x%016lX\n", length, index);
  }
  uint64_t flag = ((uint64_t)(length) << 4) | (uint64_t)(CFG_CMP_NONE);
  CFG_write_variable_u64(output, flag);
  for (size_t i = 0; i < length; i++, index++) {
    CFG_ASSERT(index < input_size);
    output.push_back(input[index]);
  }
}

size_t CFG_COMPRESS::compress_none_repeat(const uint8_t* input,
                                          const size_t input_size,
                                          std::vector<uint8_t>& output,
                                          size_t index, const uint8_t debug) {
  uint8_t compress_byte = 0;
  size_t compress_size = 0;
  size_t uncompress_size = 0;
  bool compress = false;
  size_t end_index = 0;
  while (index < input_size) {
    if (compress_size == 0 && uncompress_size == 0) {
      compress_byte = input[index];
      compress_size++;
    } else if (compress_byte == input[index]) {
      if (compress_size) {
        compress_size++;
      } else {
        CFG_ASSERT(uncompress_size >= 2);
        CFG_ASSERT(index >= uncompress_size);
        pack_data(input, input_size, output, index - uncompress_size,
                  uncompress_size - 1, debug);
        compress_size = 2;
        uncompress_size = 0;
        compress = true;
        end_index = index + (uncompress_size - 1);
        break;
      }
    } else {
      if (compress_size == 1) {
        if ((input[index] == 0 || input[index] == 0xFF) &&
            ((index + 1) < input_size) && (input[index] == input[index + 1])) {
          for (end_index = index + 1; end_index < input_size; end_index++) {
            if (input[index] == input[end_index]) {
              compress_size++;
            } else {
              break;
            }
          }
          CFG_ASSERT(compress_size >= 2);
          if (debug) {
            printf("Compress 0x%02X followed by %ld data of 0x%02X\n",
                   compress_byte, compress_size, input[index]);
          }
          uint64_t flag = ((uint64_t)(compress_size) << 4);
          if (input[index] == 0) {
            flag |= CFG_CMP_VAR_ZERO;
          } else {
            flag |= CFG_CMP_VAR_HIGH;
          }
          CFG_write_variable_u64(output, (uint64_t)(flag));
          output.push_back(compress_byte);
          compress_size = 0;
          uncompress_size = 0;
          compress = true;
          break;
        } else {
          compress_size = 0;
          uncompress_size = 2;
          compress_byte = input[index];
        }
      } else if (compress_size) {
        CFG_ASSERT(compress_size >= 2);
        CFG_ASSERT(uncompress_size == 0);
        int followup_byte = -1;
        if (compress_byte == 0 || compress_byte == 0xFF) {
          followup_byte = int(input[index]) & 0xFF;
        }
        compress_data(output, compress_byte, compress_size, followup_byte,
                      debug, "");
        if (compress_byte == 0 || compress_byte == 0xFF) {
          // restart everything
          compress_size = 0;
          end_index = index + 1;
        } else {
          compress_byte = input[index];
          compress_size = 1;
          end_index = index;
        }
        compress = true;
        break;
      } else {
        CFG_ASSERT(compress_size == 0);
        uncompress_size++;
        compress_byte = input[index];
      }
    }
    index++;
  }
  if (!compress) {
    // If not compress it must be the end of the data
    CFG_ASSERT(index == input_size);
    CFG_ASSERT(compress_size > 0 || uncompress_size > 0);
    if (compress_size) {
      CFG_ASSERT(uncompress_size == 0);
      compress_data(output, compress_byte, compress_size, -1, debug, "");
    } else {
      CFG_ASSERT(uncompress_size >= 2);
      CFG_ASSERT(index >= uncompress_size);
      pack_data(input, input_size, output, index - uncompress_size,
                uncompress_size, debug);
    }
    end_index = index;
  }
  return end_index;
}

void CFG_COMPRESS::compress(const uint8_t* input, const size_t input_size,
                            std::vector<uint8_t>& output, size_t* header_size,
                            const bool support_followup_byte,
                            const uint8_t debug) {
  CFG_ASSERT(input != nullptr && input_size > 0);
  std::vector<uint8_t> temp_output;
  size_t index = 0;
  while (index < input_size) {
    size_t length = 0;
    size_t repeat = 0;
    analyze_repeat(input, input_size, index, length, repeat, debug);
    if (length) {
      CFG_ASSERT(repeat > 0);
      size_t chunk_pattern =
          analyze_chunk_pattern(input, input_size, index, length);
      CFG_ASSERT(chunk_pattern < CFG_CMP_INVALID);
      if (chunk_pattern == CFG_CMP_ZERO || chunk_pattern == CFG_CMP_HIGH ||
          chunk_pattern == CFG_CMP_VAR) {
        // There might be more 0's or FF's
        length = length * (repeat + 1);
        uint8_t compress_byte =
            chunk_pattern == CFG_CMP_VAR
                ? input[index]
                : (chunk_pattern == CFG_CMP_ZERO ? 0 : 0xFF);
        index += length;
        int followup_byte = -1;
        while (index < input_size) {
          if (input[index] == compress_byte) {
            length++;
          } else if (chunk_pattern == CFG_CMP_VAR) {
            break;
          } else if (!support_followup_byte) {
            break;
          } else {
            followup_byte = int(input[index]) & 0xFF;
          }
          index++;
          if (followup_byte != -1) {
            break;
          }
        }
        compress_data(temp_output, compress_byte, length, followup_byte, debug,
                      "   ");
      } else {
        compress_repeat_chunk(input, input_size, temp_output, index,
                              chunk_pattern, length, repeat);
        index += (length * (repeat + 1));
        CFG_ASSERT(index <= input_size);
        if (debug) {
          printf("   Pattern: %ld\n", chunk_pattern);
        }
      }
    } else {
      CFG_ASSERT(repeat == 0);
      index =
          compress_none_repeat(input, input_size, temp_output, index, debug);
    }
  }
  CFG_ASSERT(index == input_size);
  // Finalize
  const std::vector<uint8_t> header = {'C', 'F', 'G', '_', 'C', 'M', 'P', 0};
  size_t original_output_size = output.size();
  output.insert(output.end(), header.begin(), header.end());
  CFG_write_variable_u64(output, (uint64_t)(input_size));
  CFG_write_variable_u64(output, (uint64_t)(temp_output.size()));
  if (header_size != nullptr) {
    (*header_size) = output.size() - original_output_size;
  }
  output.insert(output.end(), temp_output.begin(), temp_output.end());
}

void CFG_COMPRESS::decompress(const uint8_t* input, const size_t input_size,
                              std::vector<uint8_t>& output,
                              const uint8_t debug) {
  CFG_ASSERT(input != nullptr && input_size > 10);
  const std::vector<uint8_t> header = {'C', 'F', 'G', '_', 'C', 'M', 'P'};
  CFG_ASSERT(memcmp(input, &header[0], 7) == 0);
  uint8_t version = header[7];
  CFG_ASSERT(version == 0);  // currently only support version 0
  size_t index = 8;
  size_t original_size = CFG_read_variable_u64(input, input_size, index);
  size_t compress_size = CFG_read_variable_u64(input, input_size, index);
  CFG_ASSERT(original_size > 0);
  CFG_ASSERT(compress_size > 0);
  CFG_ASSERT((index + compress_size) <= input_size);
  // Do not use std::vector, so that firmware can implement the same
  size_t output_original_size = output.size();
  size_t end_index = index + compress_size;
  while (index < end_index) {
    uint64_t flag = CFG_read_variable_u64(input, end_index, index);
    uint64_t pattern = flag & 0x7;
    uint64_t repeat = (flag & 0x8) != 0;
    size_t length = (size_t)(flag >> 4);
    size_t temp_output_index = output.size();
    if (pattern == CFG_CMP_NONE) {
      for (uint64_t i = 0; i < length; i++) {
        CFG_ASSERT(index < end_index);
        output.push_back(input[index]);
        index++;
      }
    } else {
      uint8_t compress_byte = 0;
      if (pattern == CFG_CMP_VAR) {
        CFG_ASSERT(index < end_index);
        compress_byte = input[index];
        index++;
      } else if (pattern == CFG_CMP_HIGH || pattern == CFG_CMP_VAR_HIGH ||
                 pattern == CFG_CMP_HIGH_VAR) {
        compress_byte = 0xFF;
      }
      if (pattern == CFG_CMP_VAR_ZERO || pattern == CFG_CMP_VAR_HIGH) {
        CFG_ASSERT(index < end_index);
        output.push_back(input[index]);
        index++;
      }
      for (size_t i = 0; i < length; i++) {
        output.push_back(compress_byte);
      }
      if (pattern == CFG_CMP_ZERO_VAR || pattern == CFG_CMP_HIGH_VAR) {
        CFG_ASSERT(index < end_index);
        output.push_back(input[index]);
        index++;
      }
      if (pattern == CFG_CMP_VAR_ZERO || pattern == CFG_CMP_VAR_HIGH ||
          pattern == CFG_CMP_ZERO_VAR || pattern == CFG_CMP_HIGH_VAR) {
        length++;
      }
    }
    CFG_ASSERT(length == (output.size() - temp_output_index));
    if (repeat) {
      size_t repeat_size =
          (size_t)(CFG_read_variable_u64(input, end_index, index));
      for (uint64_t i = 0; i < repeat_size; i++) {
        output.insert(output.end(), output.begin() + temp_output_index,
                      output.begin() + temp_output_index + length);
      }
    }
  }
  CFG_ASSERT(index == end_index);
  size_t uncompress_size = output.size() - output_original_size;
  CFG_ASSERT(uncompress_size == original_size);
}