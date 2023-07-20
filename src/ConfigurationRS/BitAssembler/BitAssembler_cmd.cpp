#include "BitAssembler_mgr.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

int main(int argc, const char** argv) {
  CFG_TIME time_begin = CFG_time_begin();
  auto arg = std::make_shared<CFGArg_BITASM>();
  int status = 0;
  if (arg->parse(argc - 1, &argv[1]) && !arg->m_help) {
    if (arg->operation == "gen_device_database") {
      if (CFG_check_file_extensions(arg->m_args[0], {".xml"}) >= 0) {
        if (CFG_check_file_extensions(arg->m_args[1], {".ddb"}) >= 0) {
          if (arg->device.size()) {
            BitAssembler_MGR::ddb_gen_database(CFG_string_tolower(arg->device),
                                               arg->m_args[0], arg->m_args[1]);
          } else {
            CFG_POST_ERR(
                "BITASM: gen_device_database operation:: device option must be "
                "specified");
          }
        } else {
          CFG_POST_ERR(
              "BITASM: gen_device_database operation:: output should be in "
              ".ddb extension");
        }
      } else {
        CFG_POST_ERR(
            "BITASM: gen_device_database operation:: input should be in .xml "
            "extension");
      }
    } else if (arg->operation == "gen_bitstream_bit") {
      if (CFG_check_file_extensions(arg->m_args[0], {".bit"}) >= 0) {
        if (CFG_check_file_extensions(arg->m_args[1], {".bit"}) >= 0) {
          BitAssembler_MGR::ddb_gen_bitstream(arg->device, arg->m_args[0],
                                              arg->m_args[1], arg->reverse);
        } else {
          CFG_POST_ERR(
              "BITASM: gen_bitstream_bit operation:: output should be in .bit "
              "extension");
        }
      } else {
        CFG_POST_ERR(
            "BITASM: gen_bitstream_bit operation:: input should be in .bit "
            "extension");
      }
    } else if (arg->operation == "gen_fabric_bitstream_xml") {
      if (CFG_check_file_extensions(arg->m_args[0], {".bit"}) >= 0) {
        if (CFG_check_file_extensions(arg->m_args[1], {".xml"}) >= 0) {
          if (arg->device.size()) {
            if (arg->protocol.size()) {
              BitAssembler_MGR::ddb_gen_fabric_bitstream_xml(
                  arg->device, arg->protocol, arg->m_args[0], arg->m_args[1],
                  arg->reverse);
            } else {
              CFG_POST_ERR(
                  "BITASM: gen_fabric_bitstream_xml operation:: protocol "
                  "option must be "
                  "specified");
            }
          } else {
            CFG_POST_ERR(
                "BITASM: gen_fabric_bitstream_xml operation:: device option "
                "must be "
                "specified");
          }

        } else {
          CFG_POST_ERR(
              "BITASM: gen_fabric_bitstream_xml operation:: output should be "
              "in .xml extension");
        }
      } else {
        CFG_POST_ERR(
            "BITASM: gen_fabric_bitstream_xml operation:: input should be in "
            ".bit extension");
      }
    } else {
      CFG_INTERNAL_ERROR("Unsupported operation %s", arg->operation.c_str());
    }
  }
  CFG_POST_MSG("BITASM elapsed time: %.3f seconds",
               CFG_time_elapse(time_begin));
  return status;
}
