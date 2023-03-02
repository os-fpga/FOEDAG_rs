/*Copyright 2021 The Foedag teamGPL LicenseCopyright (c) 2021 The
 * Open-Source FPGA FoundationThis program is free software: you can
 * redistribute it and/or modifyit under the terms of the GNU General Public
 * License as published bythe Free Software Foundation, either version 3 of the
 * License, or(at your option) any later version.This program is distributed
 * in the hope that it will be useful,but WITHOUT ANY WARRANTY; without even
 * the implied warranty ofMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See theGNU General Public License for more details.You should have
 * received a copy of the GNU General Public Licensealong with this program. If
 * not, see <http://www.gnu.org/licenses/>. */
#ifndef BITASSEMBLER_MGR_H #define BITASSEMBLER_MGR_H #include <
    string> #include <vector> #include
    "CFGObject/CFGObject_auto.h" class BitAssembler_MGR {
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