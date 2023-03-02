#include <iostream>
#include <fstream>
#include "CFGCommon/CFGHelper.h"
#include "BitAssembler_mgr.h"
#include <stdio.h>

BitAssembler_MGR::BitAssembler_MGR() {
  CFG_INTERNAL_ERROR("This constructor is not supported");
}

BitAssembler_MGR::BitAssembler_MGR
(
  const std::string& project_path, 
  const std::string& device
) :
  m_project_path(project_path),
  m_device(device) {
  CFG_ASSERT(m_project_path.size());
  CFG_ASSERT(m_device.size());
}

void BitAssembler_MGR::get_fcb
(
  const CFGObject_BITOBJ_FCB * fcb
) {
  // For FCB, compiler already generate the full data in text format: fabric_bitstream.bit
  // Manager just read it and convert it to binary
  // Nothing much need to be done
  CFG_ASSERT(fcb->get_object_count() == 0);
  
  // Read fabric_bitstream.bit as text line by line, parse info out
  std::string filepath = CFG_print("%s/fabric_bitstream.bit", m_project_path.c_str());
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
          fcb->write_u32("length", static_cast<uint32_t>(std::stoul(line)));
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
          fcb->write_u32("width", static_cast<uint32_t>(std::stoul(line)));
          CFG_ASSERT(fcb->check_exist("width"));
          CFG_ASSERT(fcb->width);
        } else {
          // Unknown -- put it into warning
          m_warnings.push_back(CFG_print("FCB Parser :: unknown :: %s", line.c_str()));
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

template <typename T> uint32_t BitAssembler_MGR::get_bitline_into_bytes
(
  T start, 
  T end,
  std::vector<uint8_t>& bytes
) {
  CFG_ASSERT(start != end);
  uint32_t index = 0;
  while (start != end) {
    if ((index % 8) == 0) {
      bytes.push_back(0);
    }
    if (*start == '1') {
      bytes.back() |= (1 << (index & 7));
    } else {
      CFG_ASSERT(*start == '0');
    }
    start++;
    index++;
  }
  return index;
}

uint32_t BitAssembler_MGR::get_bitline_into_bytes
(
  const std::string& line, 
  std::vector<uint8_t>& bytes, 
  const uint32_t expected_bit, 
  const bool lsb
) {
  CFG_ASSERT(line.size());
  CFG_ASSERT(expected_bit == 0 || expected_bit == line.size());
  return (lsb ? get_bitline_into_bytes(line.begin(), line.end(), bytes) : get_bitline_into_bytes(line.rbegin(), line.rend(), bytes));
}
