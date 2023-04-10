#include "BitGenerator.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

int main(int argc, const char** argv) {
  auto arg = std::make_shared<CFGArg_BITGEN>();
  int status = 0;
  if (arg->parse(argc - 1, &argv[1]) && !arg->m_help) {
    if (CFG_find_string_in_vector({"gen_bitstream", "parse"}, arg->operation) >=
        0) {
      if (arg->m_args.size() == 2) {
        CFGCommon_ARG cmdarg;
        cmdarg.arg = arg;
        BitGenerator_entry(&cmdarg);
      } else {
        CFG_POST_ERR("BITGEN: %s operation need input and output arguments",
                     arg->operation.c_str());
        status = 1;
      }
    } else {
      CFG_POST_ERR("BITGEN: Invalid operation %s", arg->operation.c_str());
      status = 1;
    }
  }
  return status;
}
