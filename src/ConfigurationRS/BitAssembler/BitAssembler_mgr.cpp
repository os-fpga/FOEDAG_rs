#include "BitAssembler_mgr.h"

#include <fstream>
#include <iostream>

#include "CFGCommonRS/CFGCommonRS.h"
#include "nlohmann_json/json.hpp"

#define PCB_BIT_SIZE (36 * 1024)

BitAssembler_MGR::BitAssembler_MGR() {
  CFG_INTERNAL_ERROR("This constructor is not supported");
}

BitAssembler_MGR::BitAssembler_MGR(const std::string& project_path,
                                   const std::string& device)
    : m_project_path(project_path), m_device(device) {
  CFG_ASSERT(m_project_path.size());
  CFG_ASSERT(m_device.size());
}

void BitAssembler_MGR::get_scan_chain_fcb(
    const CFGObject_BITOBJ_SCAN_CHAIN_FCB* fcb) {
  // For FCB, compiler already generate the full data in text format:
  // fabric_bitstream.bit Manager just read it and convert it to binary. Nothing
  // much need to be done
  CFG_ASSERT(fcb->get_object_count() == 0);

  // Read fabric_bitstream.bit as text line by line, parse info out
  std::string filepath =
      CFG_print("%s/fabric_bitstream.bit", m_project_path.c_str());
  std::fstream file;
  file.open(filepath.c_str(), std::ios::in);
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", filepath.c_str());
  std::string line = "";
  size_t line_tracking = 0;
  size_t data_line = 0;
  bool lsb = false;
  std::vector<uint8_t> data;
  while (getline(file, line)) {
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0) {
      // allow blank line
      continue;
    }
    // Strict checking on the format
    if (line_tracking == 0) {
      // First line must start with this keyword
      CFG_ASSERT(line == "// Fabric bitstream");
      line_tracking++;
    } else if (line_tracking == 2) {
      // This will be all data, no exception
      get_bitline_into_bytes(line, data, fcb->width, lsb);
      data_line++;
    } else {
      if (line.find("//") == 0) {
        if (line.find("// Version:") == 0 || line.find("// Date:") == 0) {
          // Ignore this
          continue;
        } else if (line.find("// Bitstream length:") == 0) {
          // Should only define once
          CFG_ASSERT(!fcb->check_exist("length"));
          CFG_ASSERT(fcb->length == 0);
          line.erase(0, 20);
          CFG_get_rid_leading_whitespace(line);
          fcb->write_u32("length",
                         (uint32_t)(CFG_convert_string_to_u64(line, true)));
          CFG_ASSERT(fcb->check_exist("length"));
          CFG_ASSERT(fcb->length);
        } else if (line.find("// Bitstream width ") == 0) {
          // Should only define once
          CFG_ASSERT(!fcb->check_exist("width"));
          CFG_ASSERT(fcb->width == 0);
          line.erase(0, 19);
          lsb = line.find("(LSB -> MSB):") == 0;
          CFG_ASSERT(lsb || line.find("(MSB -> LSB):") == 0);
          line.erase(0, 13);
          CFG_get_rid_leading_whitespace(line);
          fcb->write_u32("width",
                         (uint32_t)(CFG_convert_string_to_u64(line, true)));
          CFG_ASSERT(fcb->check_exist("width"));
          CFG_ASSERT(fcb->width);
        } else {
          // Unknown -- put it into warning
          m_warnings.push_back(
              CFG_print("FCB Parser :: unknown :: %s", line.c_str()));
        }
      } else {
        // Start of data
        // Make sure length and width is known
        CFG_ASSERT(fcb->check_exist("length") && fcb->check_exist("width"));
        get_bitline_into_bytes(line, data, fcb->width, lsb);
        line_tracking++;
        data_line++;
      }
    }
  }
  CFG_ASSERT(fcb->check_exist("length") && fcb->check_exist("width"));
  CFG_ASSERT(fcb->length == data_line);
  fcb->write_u8s("data", data);
  file.close();
}

void BitAssembler_MGR::get_ql_membank_fcb(
    const CFGObject_BITOBJ_QL_MEMBANK_FCB* fcb) {
  // For FCB, compiler already generate the full data in text format:
  // fabric_bitstream.bit Manager just read it and convert it to binary. Nothing
  // much need to be done
  CFG_ASSERT(fcb->get_object_count() == 0);

  // Read fabric_bitstream.bit as text line by line, parse info out
  std::string filepath =
      CFG_print("%s/fabric_bitstream.bit", m_project_path.c_str());
  std::fstream file;
  file.open(filepath.c_str(), std::ios::in);
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", filepath.c_str());
  std::string line = "";
  size_t line_tracking = 0;
  size_t data_line = 0;
  bool lsb = false;
  uint32_t one_hot_wl = 0;
  bool wl_increasing = false;
  std::vector<uint8_t> data;
  std::vector<uint8_t> mask;
  while (getline(file, line)) {
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0) {
      // allow blank line
      continue;
    }
    // Strict checking on the format
    if (line_tracking == 0) {
      // First line must start with this keyword
      CFG_ASSERT(line == "// Fabric bitstream");
      line_tracking++;
    } else if (line_tracking == 2) {
      // This will be all data, no exception
      get_wl_bitline_into_bytes(
          line, data, mask, fcb->bl, fcb->wl,
          wl_increasing ? data_line : fcb->wl - data_line - 1, lsb);
      data_line++;
    } else {
      if (line.find("//") == 0) {
        if (line.find("// Version:") == 0 || line.find("// Date:") == 0) {
          // Ignore this
          continue;
        } else if (line.find("// Bitstream length:") == 0) {
          // Should only define once
          CFG_ASSERT(!fcb->check_exist("wl"));
          CFG_ASSERT(fcb->wl == 0);
          line.erase(0, 20);
          CFG_get_rid_leading_whitespace(line);
          fcb->write_u32("wl",
                         (uint32_t)(CFG_convert_string_to_u64(line, true)));
          CFG_ASSERT(fcb->check_exist("wl"));
          CFG_ASSERT(fcb->wl);
        } else if (line.find("// Bitstream width ") == 0) {
          // Should only define once
          CFG_ASSERT(!fcb->check_exist("bl"));
          CFG_ASSERT(fcb->bl == 0);
          line.erase(0, 19);
          lsb = line.find("(LSB -> MSB):") == 0;
          CFG_ASSERT(lsb || line.find("(MSB -> LSB):") == 0);
          line.erase(0, 13);
          CFG_get_rid_leading_whitespace(line);
          // Expected format <bl_address 5273 bits><wl_address 3407 bits>
          std::vector<std::string> words = CFG_split_string(line, " ");
          CFG_ASSERT(words.size() == 5);
          CFG_ASSERT(words[0] == "<bl_address");
          CFG_ASSERT(words[2] == "bits><wl_address");
          CFG_ASSERT(words[4] == "bits>");
          fcb->write_u32("bl",
                         (uint32_t)(CFG_convert_string_to_u64(words[1], true)));
          CFG_ASSERT(fcb->check_exist("bl"));
          CFG_ASSERT(fcb->bl);
          CFG_ASSERT(fcb->wl ==
                     (uint32_t)(CFG_convert_string_to_u64(words[3], true)));
        } else {
          // Unknown -- put it into warning
          m_warnings.push_back(
              CFG_print("FCB Parser :: unknown :: %s", line.c_str()));
        }
      } else {
        // Start of data
        // Make sure BL and WL is known
        CFG_ASSERT(fcb->check_exist("wl") && fcb->check_exist("bl"));
        get_wl_bitline_into_bytes(line, data, mask, fcb->bl, fcb->wl, 0, lsb,
                                  &one_hot_wl);
        CFG_ASSERT(one_hot_wl == 0 || one_hot_wl == (fcb->wl - 1));
        wl_increasing = one_hot_wl == 0;
        line_tracking++;
        data_line++;
      }
    }
  }
  CFG_ASSERT(fcb->check_exist("wl") && fcb->check_exist("bl"));
  CFG_ASSERT(fcb->wl == data_line);
  CFG_ASSERT(data.size() == mask.size());
  fcb->write_u8s("data", data);
  fcb->write_u8s("mask", mask);
  file.close();
}

void BitAssembler_MGR::get_icb(const CFGObject_BITOBJ_ICB* icb) {
  CFG_ASSERT(icb->get_object_count() == 0);

  // Read io_bitstream.bit as text line by line, parse info out
  std::string filepath =
      CFG_print("%s/io_bitstream.bit", m_project_path.c_str());
  // ToDO: For now IO is not fully ready, just post warning if it does not.
  //       Once IO is ready, we will need to flag error
  if (std::filesystem::exists(filepath)) {
    std::fstream file;
    file.open(filepath.c_str(), std::ios::in);
    CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", filepath.c_str());
    std::string line = "";
    size_t line_tracking = 0;
    size_t data_line = 0;
    std::string format = "";
    std::vector<uint8_t> data;
    while (getline(file, line)) {
      // Only trim the trailing whitespace
      CFG_get_rid_trailing_whitespace(line);
      if (line.size() == 0) {
        // allow blank line
        continue;
      }
      // Strict checking on the format
      if (line_tracking == 0) {
        // First line must start with this keyword
        CFG_ASSERT(line == "// Feature Bitstream: IO");
        line_tracking++;
      } else if (line_tracking == 2) {
        // This will be all data, no exception
        CFG_ASSERT(data_line < icb->bits);
        if (line == "1") {
          data[data_line >> 3] |= (1 << (data_line & 7));
        } else {
          CFG_ASSERT(line == "0");
        }
        data_line++;
      } else {
        if (line.find("//") == 0) {
          if (line.find("// Model:") == 0 || line.find("// Timestamp:") == 0) {
            // Ignore this
            continue;
          } else if (line.find("// Total Bits:") == 0) {
            // Should only define once
            CFG_ASSERT(!icb->check_exist("bits"));
            CFG_ASSERT(icb->bits == 0);
            line.erase(0, 14);
            CFG_get_rid_leading_whitespace(line);
            icb->write_u32("bits",
                           (uint32_t)(CFG_convert_string_to_u64(line, true)));
            CFG_ASSERT(icb->check_exist("bits"));
            CFG_ASSERT(icb->bits);
          } else if (line.find("// Format:") == 0) {
            CFG_ASSERT(format.empty());
            line.erase(0, 10);
            CFG_get_rid_leading_whitespace(line);
            format = line;
            CFG_ASSERT(format == "BIT");
          } else {
            // Unknown -- put it into warning
            m_warnings.push_back(
                CFG_print("ICB Parser :: unknown :: %s", line.c_str()));
          }
        } else {
          // Start of data
          // Make sure length and width is known
          CFG_ASSERT(icb->check_exist("bits"));
          CFG_ASSERT(format == "BIT");
          CFG_ASSERT(data.size() == 0);
          data.resize((icb->bits + 7) / 8);
          memset(&data[0], 0, data.size());
          if (line == "1") {
            data[data_line >> 3] |= (1 << (data_line & 7));
          } else {
            CFG_ASSERT(line == "0");
          }
          line_tracking++;
          data_line++;
        }
      }
    }
    CFG_ASSERT(icb->check_exist("bits"));
    CFG_ASSERT(icb->bits == data_line);
    icb->write_u8s("data", data);
    file.close();
  } else {
    CFG_POST_WARNING("IO bitstream file %s does not exist. Skip for now",
                     filepath.c_str());
  }
}

void BitAssembler_MGR::get_pcb(CFGObject_BITOBJ& bitobj) {
  CFG_ASSERT(bitobj.pcb.size() == 0);

  // Read bram_bitstream.json as a JSON file
  std::string filepath =
      CFG_print("%s/bram_bitstream.json", m_project_path.c_str());
  // ToDO: For now IO is not fully ready, just post warning if it does not.
  //       Once IO is ready, we will need to flag error
  if (std::filesystem::exists(filepath)) {
    std::fstream jsonfile(filepath.c_str());
    CFG_ASSERT_MSG(jsonfile.is_open() && jsonfile.good(), "Fail to open %s",
                   filepath.c_str());
    nlohmann::json json = nlohmann::json::parse(jsonfile);
    jsonfile.close();
    CFG_ASSERT(json.is_object());
    CFG_ASSERT(json.contains("bram"));
    nlohmann::json& bram_json = json["bram"];
    CFG_ASSERT(bram_json.is_array());
    bool found = false;
    for (nlohmann::json& pb : bram_json) {
      CFG_ASSERT(pb.is_object());
      CFG_ASSERT(pb.contains("pb"));
      nlohmann::json& pb_name = pb["pb"];
      CFG_ASSERT(pb_name.is_string());
      if (std::string(pb_name) == "bram.bram_lr[mem_36K_tdp].mem_36K") {
        CFG_ASSERT(pb.contains("grid"));
        CFG_ASSERT(pb["grid"].is_array());
        for (nlohmann::json& grid : pb["grid"]) {
          CFG_ASSERT(grid.contains("x"));
          CFG_ASSERT(grid.contains("y"));
          CFG_ASSERT(grid["x"].is_number_integer() ||
                     grid["x"].is_number_unsigned());
          CFG_ASSERT(grid["y"].is_number_integer() ||
                     grid["y"].is_number_unsigned());
          uint32_t x = (uint32_t)(grid["x"]);
          uint32_t y = (uint32_t)(grid["y"]);
          bitobj.create_child("pcb");
          bitobj.pcb.back()->write_u32("x", x);
          bitobj.pcb.back()->write_u32("y", y);
          bitobj.pcb.back()->write_u32("bits", PCB_BIT_SIZE);
          std::vector<uint8_t> data((PCB_BIT_SIZE + 7) / 8, 0);
          if (grid.contains("data")) {
            CFG_ASSERT(grid["data"].is_string());
            std::string bram = std::string(grid["data"]);
            CFG_ASSERT(bram.size() == PCB_BIT_SIZE);
            size_t index = 0;
            for (auto iter = bram.rbegin(); iter != bram.rend(); iter++) {
              CFG_ASSERT(*iter == '0' || *iter == '1');
              if (*iter == '1') {
                data[index >> 3] |= (1 << (index & 7));
              }
              index++;
            }
          }
          bitobj.pcb.back()->write_u8s("data", data);
        }
        found = true;
        break;
      }
    }
    CFG_ASSERT(found);
  } else {
    CFG_POST_WARNING("IO bitstream file %s does not exist. Skip for now",
                     filepath.c_str());
  }
}

void BitAssembler_MGR::get_one_region_ccff_fcb(const std::string& filepath,
                                               std::vector<uint8_t>& data) {
  CFG_ASSERT(data.size() == 0);
  // Read fabric_bitstream.bit as text line by line, parse info out
  std::fstream file;
  file.open(filepath.c_str(), std::ios::in);
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", filepath.c_str());
  std::string line = "";
  size_t line_tracking = 0;
  bool lsb = false;
  uint32_t length = 0;
  uint32_t width = 0;
  while (getline(file, line)) {
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0) {
      // allow blank line
      continue;
    }
    // Strict checking on the format
    if (line_tracking == 0) {
      // First line must start with this keyword
      CFG_ASSERT(line == "// Fabric bitstream");
      line_tracking++;
    } else if (line_tracking == 2) {
      // This will be all data, no exception
      /*
      BitAssembler_MGR_read_one_bit_in_line(data, line, data_line, byte_index,
                                            bit_index);
      */
      data.push_back(line == "1");
    } else {
      if (line.find("//") == 0) {
        if (line.find("// Version:") == 0 || line.find("// Date:") == 0) {
          // Ignore this
          continue;
        } else if (line.find("// Bitstream length:") == 0) {
          // Should only define once
          CFG_ASSERT(length == 0);
          line.erase(0, 20);
          CFG_get_rid_leading_whitespace(line);
          length = (uint32_t)(CFG_convert_string_to_u64(line, true));
          CFG_ASSERT(length);
        } else if (line.find("// Bitstream width ") == 0) {
          // Should only define once
          CFG_ASSERT(width == 0);
          line.erase(0, 19);
          lsb = line.find("(LSB -> MSB):") == 0;
          CFG_ASSERT(lsb || line.find("(MSB -> LSB):") == 0);
          line.erase(0, 13);
          CFG_get_rid_leading_whitespace(line);
          width = (uint32_t)(CFG_convert_string_to_u64(line, true));
          CFG_ASSERT(width == 1);
        } else {
          // Unknown -- put it into warning
          CFG_POST_WARNING("FCB Parser :: unknown :: %s", line.c_str());
        }
      } else {
        // Start of data
        // Make sure length and width is known
        /*
        BitAssembler_MGR_read_one_bit_in_line(data, line, data_line, byte_index,
                                              bit_index);
        */
        data.push_back(line == "1");
      }
    }
  }
  CFG_ASSERT(length > 0 && width > 0);
  file.close();
}

std::string BitAssembler_MGR::get_ocla_design(const std::string& filepath) {
  CFG_ASSERT(CFG_check_file_extensions(filepath, {".bitasm"}) == 0);
  // Read the BitObj file
  CFGObject_BITOBJ bitobj;
  CFG_ASSERT(bitobj.read(filepath));
  return bitobj.ocla;
}

template <typename T>
uint32_t BitAssembler_MGR::get_bitline_into_bytes(
    T& start, T& end, std::vector<uint8_t>& bytes,
    std::vector<uint8_t>* mask_bytes, uint32_t size) {
  CFG_ASSERT(start != end);
  uint32_t index = 0;
  while (start != end) {
    if ((index % 8) == 0) {
      bytes.push_back(0);
    }
    if (mask_bytes != nullptr && (index % 8) == 0) {
      mask_bytes->push_back(0);
    }
    if (*start == '1') {
      bytes.back() |= (1 << (index & 7));
    } else {
      CFG_ASSERT(*start == '0' || *start == 'x');
    }
    if (mask_bytes != nullptr && (*start == '0' || *start == '1')) {
      mask_bytes->back() |= (1 << (index & 7));
    }
    start++;
    index++;
    if (index == size) {
      break;
    }
  }
  return index;
}

uint32_t BitAssembler_MGR::get_bitline_into_bytes(const std::string& line,
                                                  std::vector<uint8_t>& bytes,
                                                  const uint32_t expected_bit,
                                                  const bool lsb) {
  CFG_ASSERT(line.size());
  CFG_ASSERT(expected_bit == 0 || expected_bit == line.size());
  uint32_t bits = 0;
  if (lsb) {
    auto start = line.begin();
    auto end = line.end();
    bits = get_bitline_into_bytes(start, end, bytes, nullptr);
    CFG_ASSERT(start == end);
  } else {
    auto start = line.rbegin();
    auto end = line.rend();
    bits = get_bitline_into_bytes(start, end, bytes, nullptr);
    CFG_ASSERT(start == end);
  }
  return bits;
}

uint32_t BitAssembler_MGR::get_wl_bitline_into_bytes(
    const std::string& line, std::vector<uint8_t>& bytes,
    std::vector<uint8_t>& mask_bytes, const uint32_t expected_bl_bit,
    const uint32_t expected_wl_bit, const uint32_t expected_wl, const bool lsb,
    uint32_t* one_hot_wl) {
  CFG_ASSERT(line.size());
  CFG_ASSERT(expected_bl_bit);
  CFG_ASSERT(expected_wl_bit);
  CFG_ASSERT((expected_bl_bit + expected_wl_bit) == (uint32_t)(line.size()));
  std::vector<uint8_t> wl;
  uint32_t bl_size = 0;
  if (lsb) {
    auto start = line.begin();
    auto end = line.end();
    bl_size =
        get_bitline_into_bytes(start, end, bytes, &mask_bytes, expected_bl_bit);
    CFG_ASSERT(bl_size == expected_bl_bit);
    get_bitline_into_bytes(start, end, wl, nullptr, expected_wl_bit);
    CFG_ASSERT(start == end);
  } else {
    auto start = line.rbegin();
    auto end = line.rend();
    get_bitline_into_bytes(start, end, wl, nullptr, expected_wl_bit);
    bl_size =
        get_bitline_into_bytes(start, end, bytes, &mask_bytes, expected_bl_bit);
    CFG_ASSERT(bl_size == expected_bl_bit);
    CFG_ASSERT(start == end);
  }
  if (one_hot_wl == nullptr) {
    CFG_ASSERT(expected_wl < expected_wl_bit);
    for (uint32_t i = 0; i < expected_wl_bit; i++) {
      if (wl[i >> 3] & (1 << (i & 7))) {
        CFG_ASSERT(i == expected_wl);
      } else {
        CFG_ASSERT(i != expected_wl);
      }
    }
  } else {
    (*one_hot_wl) = expected_wl_bit;
    for (uint32_t i = 0; i < expected_wl_bit; i++) {
      if (wl[i >> 3] & (1 << (i & 7))) {
        // Can only set once
        CFG_ASSERT((*one_hot_wl) == expected_wl_bit);
        (*one_hot_wl) = i;
      }
    }
    // Must have one-hot-bit
    CFG_ASSERT((*one_hot_wl) < expected_wl_bit);
  }
  return bl_size;
}
