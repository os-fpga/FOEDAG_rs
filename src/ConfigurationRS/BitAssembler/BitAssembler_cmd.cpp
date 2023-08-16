#include "BitAssembler_mgr.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

int main(int argc, const char** argv) {
  CFG_TIME time_begin = CFG_time_begin();
  auto arg = std::make_shared<CFGArg_BITASM>();
  int status = 0;
  if (arg->parse(argc - 1, &argv[1]) && !arg->m_help) {
    if (arg->get_sub_arg_name() == "gen_device_database") {
      const CFGArg_BITASM_GEN_DEVICE_DATABASE* subarg =
          static_cast<const CFGArg_BITASM_GEN_DEVICE_DATABASE*>(
              arg->get_sub_arg());
      if (CFG_check_file_extensions(subarg->m_args[0], {".xml"}) >= 0) {
        if (CFG_check_file_extensions(subarg->m_args[1], {".ddb"}) >= 0) {
          BitAssembler_MGR::ddb_gen_database(subarg->device, subarg->m_args[0],
                                             subarg->m_args[1]);
        } else {
          CFG_POST_ERR(
              "BITASM: gen_device_database:: output should be in .ddb "
              "extension");
        }
      } else {
        CFG_POST_ERR(
            "BITASM: gen_device_database:: input should be in .xml extension");
      }
    } else if (arg->get_sub_arg_name() == "gen_bitstream_bit") {
      const CFGArg_BITASM_GEN_BITSTREAM_BIT* subarg =
          static_cast<const CFGArg_BITASM_GEN_BITSTREAM_BIT*>(
              arg->get_sub_arg());
      if (CFG_check_file_extensions(subarg->m_args[0], {".bit"}) >= 0) {
        if (CFG_check_file_extensions(subarg->m_args[1], {".bit"}) >= 0) {
          BitAssembler_MGR::ddb_gen_bitstream(subarg->device, subarg->m_args[0],
                                              subarg->m_args[1],
                                              subarg->reverse);
        } else {
          CFG_POST_ERR(
              "BITASM: gen_bitstream_bit:: output should be in .bit extension");
        }
      } else {
        CFG_POST_ERR(
            "BITASM: gen_bitstream_bit:: input should be in .bit extension");
      }
    } else if (arg->get_sub_arg_name() == "gen_bitstream_xml") {
      const CFGArg_BITASM_GEN_BITSTREAM_XML* subarg =
          static_cast<const CFGArg_BITASM_GEN_BITSTREAM_XML*>(
              arg->get_sub_arg());
      if (CFG_check_file_extensions(subarg->m_args[0], {".bit"}) >= 0) {
        if (CFG_check_file_extensions(subarg->m_args[1], {".xml"}) >= 0) {
          BitAssembler_MGR::ddb_gen_fabric_bitstream_xml(
              subarg->device, subarg->protocol, subarg->m_args[0],
              subarg->m_args[1], subarg->reverse);
        } else {
          CFG_POST_ERR(
              "BITASM: gen_bitstream_xml:: output should be in .xml extension");
        }
      } else {
        CFG_POST_ERR(
            "BITASM: gen_bitstream_xml:: input should be in .bit extension");
      }
    } else {
      CFG_INTERNAL_ERROR("Invalid sub command %s",
                         arg->get_sub_arg_name().c_str());
    }
  }
  CFG_POST_MSG("BITASM elapsed time: %.3f seconds",
               CFG_time_elapse(time_begin));
  return status;
}
