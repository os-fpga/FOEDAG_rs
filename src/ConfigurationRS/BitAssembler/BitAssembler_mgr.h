#ifndef BITASSEMBLER_MGR_H
#define BITASSEMBLER_MGR_H

#include <string>
#include <vector>

#include "CFGObject/CFGObject_auto.h"

class BitAssembler_MGR {
 public:
  BitAssembler_MGR();

  BitAssembler_MGR(const std::string& project_path, const std::string& device);
  void get_fcb(const CFGObject_BITOBJ_FCB* fcb);
  std::vector<std::string> m_warnings;

 private:
  template <typename T>
  uint32_t get_bitline_into_bytes(T start, T end, std::vector<uint8_t>& bytes);
  uint32_t get_bitline_into_bytes(const std::string& line,
                                  std::vector<uint8_t>& bytes,
                                  const uint32_t expected_bit = 0,
                                  const bool lsb = true);

  const std::string m_project_path;
  const std::string m_device;
};

#endif