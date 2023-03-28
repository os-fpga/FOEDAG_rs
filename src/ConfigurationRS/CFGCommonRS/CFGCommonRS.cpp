#include "CFGCommonRS.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "CFGCompress.h"

const uint16_t CFGCommonRS_CRC_8408_TABLE[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF, 0x8C48,
    0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7, 0x1081, 0x0108,
    0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E, 0x9CC9, 0x8D40, 0xBFDB,
    0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876, 0x2102, 0x308B, 0x0210, 0x1399,
    0x6726, 0x76AF, 0x4434, 0x55BD, 0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E,
    0xFAE7, 0xC87C, 0xD9F5, 0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E,
    0x54B5, 0x453C, 0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD,
    0xC974, 0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3, 0x5285,
    0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A, 0xDECD, 0xCF44,
    0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72, 0x6306, 0x728F, 0x4014,
    0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9, 0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5,
    0xA96A, 0xB8E3, 0x8A78, 0x9BF1, 0x7387, 0x620E, 0x5095, 0x411C, 0x35A3,
    0x242A, 0x16B1, 0x0738, 0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862,
    0x9AF9, 0x8B70, 0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E,
    0xF0B7, 0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036, 0x18C1,
    0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E, 0xA50A, 0xB483,
    0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5, 0x2942, 0x38CB, 0x0A50,
    0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD, 0xB58B, 0xA402, 0x9699, 0x8710,
    0xF3AF, 0xE226, 0xD0BD, 0xC134, 0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7,
    0x6E6E, 0x5CF5, 0x4D7C, 0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1,
    0xA33A, 0xB2B3, 0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72,
    0x3EFB, 0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A, 0xE70E,
    0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1, 0x6B46, 0x7ACF,
    0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9, 0xF78F, 0xE606, 0xD49D,
    0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330, 0x7BC7, 0x6A4E, 0x58D5, 0x495C,
    0x3DE3, 0x2C6A, 0x1EF1, 0x0F78};

/*
  All about Helper
*/

uint8_t CFG_write_variable_u64(std::vector<uint8_t>& data, uint64_t value) {
  // At least one byte
  uint8_t count = 1;
  data.push_back(value & 0x7F);
  value >>= 7;
  while (value) {
    CFG_ASSERT(count < 10);

    // Set MSB bit
    data.back() |= 0x80;

    // Put new value
    data.push_back(value & 0x7F);
    value >>= 7;
    count++;
  }
  return count;
}

uint64_t CFG_read_variable_u64(const uint8_t* data, size_t data_size,
                               size_t& index, int max_size) {
  CFG_ASSERT(data != nullptr && data_size > 0);
  uint64_t u64 = 0;
  int current_index = 0;
  while (1) {
    // maximum is uint64_t (10 bytes which including next bit)
    CFG_ASSERT(current_index < 10);
    CFG_ASSERT(index < data_size);
    u64 |= (uint64_t)((uint64_t)(data[index] & 0x7F) << (current_index * 7));
    current_index++;
    if ((data[index] & 0x80) == 0) {
      index++;
      break;
    }
    index++;
  }
  CFG_ASSERT(max_size == -1 || current_index <= max_size);
  return u64;
}

void CFG_read_binary_file(const std::string& filepath,
                          std::vector<uint8_t>& data) {
  // File size to prepare memory
  std::ifstream file(filepath.c_str(), std::ios::binary | std::ios::ate);
  CFG_ASSERT(file.is_open());
  size_t filesize = file.tellg();
  file.close();

  // Read the binary
  CFG_ASSERT(filesize > 0);
  data.clear();
  data.resize(filesize);
  file = std::ifstream(filepath.c_str(), std::ios::in | std::ios::binary);
  CFG_ASSERT(file.is_open());
  file.read((char*)(&data[0]), data.size());
  file.close();
}

void CFG_write_binary_file(const std::string& filepath, const uint8_t* data,
                           const size_t data_size) {
  std::ofstream file(filepath.c_str(), std::ios::out | std::ios::binary);
  CFG_ASSERT(file.is_open());
  file.write((char*)(const_cast<uint8_t*>(data)), data_size);
  file.flush();
  file.close();
}

void CFG_compress(const uint8_t* input, const size_t input_size,
                  std::vector<uint8_t>& output, size_t* header_size,
                  const bool debug, const bool retry) {
  CFG_ASSERT(input != nullptr);
  CFG_ASSERT(input_size > 0);

  std::vector<uint8_t> cmp_output0;
  size_t header_size0 = 0;
  std::vector<uint8_t> cmp_output1;
  size_t header_size1 = 0;
  CFG_COMPRESS::compress(input, input_size, cmp_output0, &header_size0, true,
                         debug);
  if (retry) {
    CFG_COMPRESS::compress(input, input_size, cmp_output1, &header_size1, false,
                           debug);
    if (debug) {
      CFG_POST_DBG("Compressed size (with followup byte support): %ld",
                   cmp_output0.size());
      CFG_POST_DBG("Compressed size (without followup byte support): %ld",
                   cmp_output1.size());
    }
  }
  if (retry && (cmp_output0.size() > cmp_output1.size())) {
    output.insert(output.end(), cmp_output1.begin(), cmp_output1.end());
  } else {
    output.insert(output.end(), cmp_output0.begin(), cmp_output0.end());
  }
  if (header_size != nullptr) {
    if (retry && (cmp_output0.size() > cmp_output1.size())) {
      (*header_size) = header_size1;
    } else {
      (*header_size) = header_size0;
    }
  }
}

void CFG_decompress(const uint8_t* input, const size_t input_size,
                    std::vector<uint8_t>& output, const bool debug) {
  CFG_ASSERT(input != nullptr);
  CFG_ASSERT(input_size > 0);
  CFG_COMPRESS::decompress(input, input_size, output, debug);
}

uint16_t CFG_crc16(const uint8_t* addr, size_t size, uint16_t lfsr_init,
                   bool final_xor, const uint16_t* custom_table) {
  CFG_ASSERT(addr != nullptr && size > 0);
  uint16_t crc(lfsr_init);
  uint8_t table_index = 0;
  const uint16_t* TABLE =
      custom_table == nullptr ? CFGCommonRS_CRC_8408_TABLE : custom_table;
  for (size_t i = 0; i < size; i++) {
    table_index = (uint8_t(crc) ^ addr[i]) & 0xFF;
    crc = TABLE[table_index] ^ (crc >> 8);
  }
  if (final_xor) {
    crc ^= 0xFFFF;
  }
  return crc;
}

template <typename T>
static void CFG_append_T(std::vector<uint8_t>& data, T value) {
  for (size_t i = 0; i < sizeof(T); i++) {
    data.push_back(value);
    value >>= 8;
  }
}

void CFG_append_u8(std::vector<uint8_t>& data, uint8_t value) {
  CFG_append_T(data, value);
}

void CFG_append_u16(std::vector<uint8_t>& data, uint16_t value) {
  CFG_append_T(data, value);
}

void CFG_append_u32(std::vector<uint8_t>& data, uint32_t value) {
  CFG_append_T(data, value);
}

void CFG_append_u64(std::vector<uint8_t>& data, uint64_t value) {
  CFG_append_T(data, value);
}

uint64_t CFG_extract_bits(const uint8_t* data, const uint64_t total_bit_size,
                          const uint32_t bit_size, uint64_t& bit_index) {
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(total_bit_size > 0);
  CFG_ASSERT(bit_size > 0);
  uint64_t value = 0;
  for (uint32_t i = 0; i < bit_size; i++, bit_index++) {
    CFG_ASSERT(bit_index < total_bit_size);
    if (data[bit_index >> 3] & (1 << (bit_index & 7))) {
      value |= (1 << i);
    }
  }
  return value;
}

int CFG_check_file_extensions(const std::string& filepath,
                              const std::vector<std::string> extensions) {
  CFG_ASSERT(extensions.size());
  if (filepath.size()) {
    int ext = -1;
    int index = 0;
    for (auto extension : extensions) {
      if (filepath.size() > extension.size()) {
        if (filepath.rfind(extension) == (filepath.size() - extension.size())) {
          ext = index;
          break;
        }
      }
      index++;
    }
    return ext;
  }
  return -2;
}

void CFG_print_hex(std::ofstream& file, const uint8_t* data,
                   const uint64_t data_size, const uint8_t unit_size,
                   const std::string space, bool detail) {
  CFG_ASSERT(unit_size == 1 || unit_size == 2 || unit_size == 4 ||
             unit_size == 8);
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(data_size > 0);
  file << space.c_str() << "Address   |";
  uint8_t dash = 0;
  for (uint32_t i = 0; i < 16; i += unit_size) {
    file << CFG_print(" %0*X", unit_size * 2, i).c_str();
    dash += ((unit_size * 2) + 1);
  }
  file << "\n";
  file << space.c_str();
  for (uint8_t i = 0; i < (11 + dash); i++) {
    file << "-";
  }
  uint32_t total_line = (data_size + 15) / 16;
  uint32_t line_index = 0;
  uint8_t dot = 0;
  for (uint64_t i = 0; i < data_size;) {
    if ((i % 16) == 0) {
      if (total_line <= 5 || detail || (line_index < 2) ||
          line_index >= (total_line - 2)) {
        file << "\n";
        file << space.c_str() << CFG_print("%08X  |", i).c_str();
        if (dot == 2) {
          dot++;
        }
      } else if (dot == 0) {
        file << "\n" << space.c_str() << "  ....    |";
        dot++;
      }
      line_index++;
    }
    if (dot == 0 || dot == 3) {
      std::string data_string = "";
      uint8_t j = 0;
      for (j = 0; j < unit_size && i < data_size; j++, i++) {
        data_string = CFG_print("%02X%s", data[i], data_string.c_str());
      }
      for (; j < unit_size; j++) {
        data_string = CFG_print("  %s", data_string.c_str());
      }
      file << " " << data_string.c_str();
    } else if (dot == 1) {
      file << "    ....";
      dot++;
      for (uint8_t j = 0; j < unit_size && i < data_size; j++, i++) {
      }
    } else {
      for (uint8_t j = 0; j < unit_size && i < data_size; j++, i++) {
      }
    }
  }
  file << "\n";
}

void CFG_print_binary_line_by_line(std::ofstream& file, const uint8_t* data,
                                   const uint64_t total_bits,
                                   const uint64_t bit_per_line,
                                   const uint64_t data_alignment,
                                   const std::string space, bool detail) {
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(total_bits);
  CFG_ASSERT(bit_per_line);
  uint64_t max_bit_per_line = bit_per_line;
  if (data_alignment != 0) {
    while (max_bit_per_line % data_alignment) {
      max_bit_per_line++;
    }
  }
  uint64_t total_line = (total_bits + max_bit_per_line - 1) / max_bit_per_line;
  size_t max_line_string_size = CFG_print("%d", total_line).size();
  uint32_t line_index = 0;
  uint8_t dot = 0;
  for (uint64_t i = 0; i < total_bits;) {
    if ((i % max_bit_per_line) == 0) {
      if (total_line <= 5 || detail || (line_index < 2) ||
          line_index >= (total_line - 2)) {
        file << "\n";
        file
            << space.c_str()
            << CFG_print("#%*lu  | ", max_line_string_size, line_index).c_str();
        if (dot == 1) {
          dot++;
        }
      } else if (dot == 0) {
        file << "\n" << space.c_str() << "  ....    ";
        dot++;
      }
      line_index++;
    }
    if (dot == 0 || dot == 2) {
      uint64_t j = 0;
      for (; j < bit_per_line && i < total_bits; j++, i++) {
        if (data[i >> 3] & (1 << (i & 7))) {
          file << "1";
        } else {
          file << "0";
        }
      }
      while (data_alignment != 0 && (j % data_alignment) != 0) {
        j++;
        i++;
      }
    } else {
      uint64_t j = 0;
      for (; j < bit_per_line && i < total_bits; j++, i++) {
      }
      while (data_alignment != 0 && (j % data_alignment) != 0) {
        j++;
        i++;
      }
    }
  }
  file << "\n";
}
