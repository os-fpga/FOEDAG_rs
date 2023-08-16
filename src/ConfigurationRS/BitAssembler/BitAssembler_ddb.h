#ifndef BITASSEMBLER_DDB_H
#define BITASSEMBLER_DDB_H

#include <string>

#include "CFGObject/CFGObject_auto.h"

// Device Database
struct BitAssembler_DEVICE {
  std::string device = "";
  std::string family = "";
  std::string series = "";
  std::string protocol = "";
  std::string blwl = "";
};
void CFG_ddb_search_device(const std::string& filepath,
                           const std::string& device_name,
                           BitAssembler_DEVICE& device);

// DDB_00
void CFG_ddb_gen_database_00(const std::string& device,
                             const std::string& input_xml,
                             const std::string& output_ddb);
void CFG_ddb_gen_bitstream_00(const CFGObject_DDB_00* obj,
                              const std::string& input_bit,
                              const std::string& output_bit, bool reverse);
void CFG_ddb_gen_fabric_bitstream_xml_00(const CFGObject_DDB_00* ddb,
                                         const std::string& protocol,
                                         const std::string& input_bit,
                                         const std::string& output_xml,
                                         bool reverse);

#endif