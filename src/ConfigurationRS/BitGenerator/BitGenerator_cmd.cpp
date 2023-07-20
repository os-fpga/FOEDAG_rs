#include "BitGenerator.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGCrypto/CFGOpenSSL.h"

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
        CFG_POST_ERR(
            "BITGEN: %s operation:: input and output arguments must be "
            "specified",
            arg->operation.c_str());
        status = 1;
      }
    } else if (arg->operation == "gen_private_pem") {
      if (arg->signing_key.size() > 0 &&
          CFG_check_file_extensions(arg->m_args[0], {".pem"}) == 0) {
        CFGOpenSSL::gen_private_pem(arg->signing_key, arg->m_args[0],
                                    arg->passphrase, arg->no_passphrase);
      } else {
        CFG_POST_ERR(
            "BITGEN: gen_private_pem operation:: signing_key option must be "
            "specified and output should be in .pem extension");
        status = 1;
      }
    } else if (arg->operation == "gen_public_pem") {
      if (arg->m_args.size() == 2 &&
          CFG_check_file_extensions(arg->m_args[0], {".pem"}) == 0 &&
          CFG_check_file_extensions(arg->m_args[1], {".pem"}) == 0) {
        CFGOpenSSL::gen_public_pem(arg->m_args[0], arg->m_args[1],
                                   arg->passphrase);
      } else {
        CFG_POST_ERR(
            "BITGEN: gen_public_pem operation:: input and output "
            "arguments must be specified, both should be in .pem extension");
        status = 1;
      }
    } else {
      CFG_POST_ERR("BITGEN: Invalid operation %s", arg->operation.c_str());
      status = 1;
    }
  }
  return status;
}
