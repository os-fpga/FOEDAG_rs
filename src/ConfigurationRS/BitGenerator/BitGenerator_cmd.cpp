#include "BitGenerator.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

int main(int argc, const char** argv) {
  CFGArg_BITGEN bitgen_arg;
  int status = 0;
  if (bitgen_arg.parse(argc - 1, &argv[1]) && !bitgen_arg.m_help) {
    if (CFG_find_string_in_vector({"gen_bitstream", "parse"},
                                  bitgen_arg.operation) >= 0) {
      if (bitgen_arg.m_args.size() == 2) {
        CFGCommon_ARG cmdarg;
        cmdarg.arg = &bitgen_arg;
        BitGenerator_entry(&cmdarg);
      } else {
        CFG_POST_ERR("BITGEN: %s operation need input and output arguments",
                     bitgen_arg.operation.c_str());
        status = 1;
      }
    } else {
      CFG_POST_ERR("BITGEN: Invalid operation %s",
                   bitgen_arg.operation.c_str());
      status = 1;
    }
  }
  return status;
}
