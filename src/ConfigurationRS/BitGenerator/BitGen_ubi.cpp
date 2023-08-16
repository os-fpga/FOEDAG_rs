#include "BitGen_ubi.h"

#include "BitGen_analyzer.h"

bool BitGen_UBI::is_supported_bop_identifier(const std::string& identifier) {
  bool supported = false;
  int identifier_index =
      BitGen_PACKER::find_supported_bop_identifier(identifier);
  if (identifier_index == -1) {
    supported = CFG_find_string_in_vector({"MANF", "FCB", "ICB", "PCB"},
                                          identifier) >= 0;
  } else {
    supported = true;
  }
  return supported;
}

uint32_t BitGen_UBI::get_u32(const uint8_t* data) {
  uint32_t u32 = 0;
  for (size_t i = 0; i < 4; i++) {
    u32 |= (((uint32_t)(data[i]) & 0xFF) << (i * 8));
  }
  return u32;
}

int BitGen_UBI::parse_old_bop(std::vector<uint8_t>& data, bool is_last_file,
                              uint32_t* fsbl_size) {
  CFG_POST_MSG("Parsing BOP using old format");
  int count = 0;
  size_t index = 0;
  bool update_crc = false;
  while (index < data.size()) {
    size_t max_size = data.size() - index;
    if (max_size > 64) {
      std::string identifier = CFG_get_null_terminate_string(&data[index], 4);
      CFG_POST_MSG("  BOP: %s", identifier.c_str());
      if (is_supported_bop_identifier(identifier)) {
        uint32_t offset = get_u32(&data[index + 28]);
        if (offset == 0) {
          CFG_POST_MSG("    This is last BOP");
          offset = max_size;
          if (!is_last_file) {
            CFG_POST_MSG("    Overwrite Offset to Next Header");
            memcpy(&data[index + 28], &offset, sizeof(offset));
            update_crc = true;
          }
        }
        CFG_POST_MSG("    Size: %d Bytes", offset);
        if (fsbl_size != nullptr) {
          CFG_POST_MSG("    UBI FSBL End Offset: %d",
                       offset + sizeof(BitGen_UBI_HEADER));
          *fsbl_size = offset + sizeof(BitGen_UBI_HEADER);
          fsbl_size = nullptr;
        }
        uint16_t crc16 = CFG_bop_A001_crc16(&data[index], 62);
        if (update_crc) {
          CFG_POST_MSG("    Overwrite CRC16");
          memcpy(&data[index + 62], &crc16, sizeof(crc16));
          count++;
        } else {
          if (memcmp(&crc16, &data[index + 62], sizeof(crc16)) == 0) {
            count++;
          } else {
            CFG_POST_ERR("CRC Checking failed");
            count = -1;
            break;
          }
        }
        index += offset;
      } else {
        CFG_POST_ERR("Unknown identifier");
        count = -1;
        break;
      }
    } else {
      CFG_POST_ERR(
          "Do not have enough memory at offset 0x%08X to further parse the BOP",
          index);
      count = -1;
      break;
    }
  }
  return count;
}

void BitGen_UBI::package(BitGen_UBI_HEADER& header,
                         std::vector<std::string>& input_filepaths,
                         const std::string& output_filepath) {
  CFG_ASSERT(header.package_count == 0);
  CFG_ASSERT(input_filepaths.size());
  std::vector<uint8_t> data;
  data.resize(sizeof(header));
  size_t filepath_index = 0;
  bool status = true;
  for (auto& filepath : input_filepaths) {
    status = false;
    std::vector<uint8_t> input;
    CFG_read_binary_file(filepath, input);
    filepath_index++;
    bool is_last_file = filepath_index == input_filepaths.size();
    if (input.size()) {
      if (input.size() > 64) {
        std::string identifier = CFG_get_null_terminate_string(&input[0], 4);
        uint32_t crc32 = CFG_crc32(&input[0], 0x7FC);
        uint16_t crc16 = CFG_bop_A001_crc16(&input[0], 62);
        // New BOP is multiple of 2k
        if (BitGen_PACKER::find_supported_bop_identifier(identifier) >= 0 &&
            input.size() >= BitGen_BITSTREAM_BLOCK_SIZE &&
            (input.size() % BitGen_BITSTREAM_BLOCK_SIZE) == 0 &&
            memcmp(&crc32, &input[0x7FC], 4) == 0) {
          CFG_POST_MSG(
              "%s is identified as new BOP format, because the identifier, "
              "size and crc criterias met",
              filepath.c_str());
          std::string error_msg = "";
          std::vector<size_t> sizes =
              BitGen_ANALYZER::parse(input, true, true, error_msg, true);
          if (error_msg.size()) {
            break;
          } else {
            CFG_ASSERT(sizes.size());
            CFG_ASSERT(sizes.size() < 256);
            CFG_ASSERT(((size_t)(header.package_count) + sizes.size()) < 256);
            header.package_count += (uint8_t)(sizes.size());
            status = true;
          }
        } else if (is_supported_bop_identifier(identifier) &&
                   memcmp(&crc16, &input[62], 2) == 0) {
          CFG_POST_MSG(
              "%s is identified as old BOP format, because the identifier and "
              "crc criterias met",
              filepath.c_str());
          int count = parse_old_bop(
              input, is_last_file,
              (filepath_index == 1 && identifier == "FSBL") ? &header.fsbl_size
                                                            : nullptr);
          if (count == -1) {
            break;
          }
          CFG_ASSERT(count);
          CFG_ASSERT(count < 256);
          CFG_ASSERT(((int)(header.package_count) + count) < 256);
          header.package_count += (uint8_t)(count);
          status = true;
        } else {
          CFG_POST_ERR("Fail to identified the BOP for %s", filepath.c_str());
        }
      } else {
        CFG_POST_ERR(
            "%s does not have minimum file size to identify as old or new BOP "
            "format - %ld Bytes",
            filepath.c_str(), input.size());
      }
    } else {
      CFG_POST_ERR("Fail to read %s", filepath.c_str());
    }
    if (status) {
      data.insert(data.end(), input.begin(), input.end());
    }
    memset(&input[0], 0, input.size());
    input.clear();
    if (!status) {
      break;
    }
  }
  if (status) {
    CFG_POST_MSG("Total BOP image: %d", header.package_count);
    header.size = (uint32_t)(data.size());
    header.header_size = (uint16_t)(sizeof(header));
    header.crc16 = CFG_bop_A001_crc16((uint8_t*)(&header), sizeof(header) - 2);
    memcpy(&data[0], &header, sizeof(header));
    CFG_POST_MSG("Writing output %s", output_filepath.c_str());
    CFG_write_binary_file(output_filepath, &data[0], data.size());
  }
}
