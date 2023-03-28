#include "BitGenerator.h"
#include "CFGCommon/CFGArg_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

int main(int argc, const char** argv) {
  CFGArg_BITGEN bitgen_arg;
  if (bitgen_arg.parse(argc - 1, &argv[1]) && !bitgen_arg.Help) {
    CFGCommon_ARG cmdarg;
    cmdarg.arg = &bitgen_arg;
    BitGenerator_entry(&cmdarg);
  }
  return 0;
}
