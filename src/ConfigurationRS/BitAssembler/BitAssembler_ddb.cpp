#include "BitAssembler_ddb.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

#include "BitAssembler_mgr.h"
#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGObject/CFGObject_auto.h"

const std::map<std::string, std::vector<std::string>> DDB_TYPES_DATABASE = {
    {"DDB_00", {"1ge100-es1", "gemini_compact_10x8"}}};

static std::string CFG_ddb_get_type(std::string device) {
  std::string ddb = "";
  CFG_string_tolower(device);
  for (auto& db : DDB_TYPES_DATABASE) {
    if (CFG_find_string_in_vector(db.second, device) != -1) {
      ddb = db.first;
      break;
    }
  }
  return ddb;
}

CFGObject* CFG_ddb_get_database(std::string device) {
  CFGObject* ddb = nullptr;
  if (CFG_check_file_extensions(device, {".ddb"}) == 0) {
  } else {
    CFG_INTERNAL_ERROR("Does not support conversion device database (for now)");
  }
  std::vector<uint8_t> data;
  CFG_read_binary_file(device, data);
  CFG_ASSERT(data.size() >= 8);
  size_t index = 0;
  std::string ddb_type =
      CFG_get_string_from_bytes(&data[0], data.size(), index, 8, 2, 8);
  CFG_ASSERT(DDB_TYPES_DATABASE.find(ddb_type) != DDB_TYPES_DATABASE.end());
  if (ddb_type == "DDB_00") {
    CFGObject_DDB_00* ddb0 = CFG_MEM_NEW(CFGObject_DDB_00);
    ddb0->read(data);
    ddb = ddb0;
  } else {
    CFG_INTERNAL_ERROR("Unsupported DDB type %s", ddb_type.c_str());
  }
  return ddb;
};

void BitAssembler_MGR::ddb_gen_database(const std::string& device,
                                        const std::string& input_xml,
                                        const std::string& output_ddb) {
  std::string ddb_type = CFG_ddb_get_type(device);
  CFG_ASSERT_MSG(ddb_type.size(),
                 "Device %s does not support device database generation",
                 device.c_str());
  if (ddb_type == "DDB_00") {
    CFG_ddb_gen_database_00(device, input_xml, output_ddb);
  } else {
    CFG_INTERNAL_ERROR("Unsupported device database type");
  }
}

void BitAssembler_MGR::ddb_gen_bitstream(const std::string& device,
                                         const std::string& input_bit,
                                         const std::string& output_bit,
                                         bool reverse) {
  CFGObject* ddb = CFG_ddb_get_database(device);
  if (ddb->get_name() == "DDB_00") {
    CFGObject_DDB_00* ddb0 = reinterpret_cast<CFGObject_DDB_00*>(ddb);
    CFG_ddb_gen_bitstream_00(ddb0, input_bit, output_bit, reverse);
    CFG_MEM_DELETE(ddb0);
  } else {
    CFG_INTERNAL_ERROR("Unsupported DDB type %s", ddb->get_name().c_str());
  }
}

void BitAssembler_MGR::ddb_gen_fabric_bitstream_xml(
    const std::string& device, const std::string& protocol,
    const std::string& input_bit, const std::string& output_xml, bool reverse) {
  CFGObject* ddb = CFG_ddb_get_database(device);
  if (ddb->get_name() == "DDB_00") {
    CFGObject_DDB_00* ddb0 = reinterpret_cast<CFGObject_DDB_00*>(ddb);
    CFG_ddb_gen_fabric_bitstream_xml_00(ddb0, protocol, input_bit, output_xml,
                                        reverse);
    CFG_MEM_DELETE(ddb0);
  } else {
    CFG_INTERNAL_ERROR("Unsupported DDB type %s", ddb->get_name().c_str());
  }
}