#ifndef BITASSEMBLER_MGR_H
#define BITASSEMBLER_MGR_H

#include <string>
#include <vector>

#include "CFGObject/CFGObject_auto.h"

class BitAssembler_MGR {
 public:
  BitAssembler_MGR();

  BitAssembler_MGR(const std::string& project_path, const std::string& device);
  void get_scan_chain_fcb(const CFGObject_BITOBJ_SCAN_CHAIN_FCB* fcb);
  void get_ql_membank_fcb(const CFGObject_BITOBJ_QL_MEMBANK_FCB* fcb);
  std::vector<std::string> m_warnings;

  // public static
 public:
  static void ddb_gen_database(const std::string& device,
                               const std::string& input_xml,
                               const std::string& output_ddb);
  static void ddb_gen_bitstream(const std::string& device,
                                const std::string& input_bit,
                                const std::string& output_bit, bool reverse);
  static void ddb_gen_fabric_bitstream_xml(const std::string& device,
                                           const std::string& protocol,
                                           const std::string& input_bit,
                                           const std::string& output_xml,
                                           bool reverse);
  static void get_one_region_ccff_fcb(const std::string& filepath,
                                      std::vector<uint8_t>& data);

 private:
  template <typename T>
  uint32_t get_bitline_into_bytes(T& start, T& end, std::vector<uint8_t>& bytes,
                                  uint32_t size = 0);
  uint32_t get_bitline_into_bytes(const std::string& line,
                                  std::vector<uint8_t>& bytes,
                                  const uint32_t expected_bit = 0,
                                  const bool lsb = true);
  uint32_t get_wl_bitline_into_bytes(const std::string& line,
                                     std::vector<uint8_t>& bytes,
                                     const uint32_t expected_bl_bit,
                                     const uint32_t expected_wl_bit,
                                     const uint32_t expected_wl,
                                     const bool lsb = true);
  const std::string m_project_path;
  const std::string m_device;
};

#endif