#include "BitGen_analyzer.h"
#include "BitGen_ubi.h"
#include "BitGenerator.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGCrypto/CFGOpenSSL.h"

int main(int argc, const char** argv) {
  auto arg = std::make_shared<CFGArg_BITGEN>();
  int status = 0;
  if (arg->parse(argc - 1, &argv[1]) && !arg->m_help) {
    if (CFG_find_string_in_vector({"gen_bitstream", "parse"},
                                  arg->get_sub_arg_name()) >= 0) {
      CFGCommon_ARG cmdarg;
      cmdarg.arg = arg;
      status = BitGenerator_entry(&cmdarg) ? 0 : 1;
    } else if (arg->get_sub_arg_name() == "gen_private_pem") {
      const CFGArg_BITGEN_GEN_PRIVATE_PEM* subarg =
          static_cast<const CFGArg_BITGEN_GEN_PRIVATE_PEM*>(arg->get_sub_arg());
      if (CFG_check_file_extensions(subarg->m_args[0], {".pem"}) == 0) {
        CFGOpenSSL::gen_private_pem(subarg->signing_key, subarg->m_args[0],
                                    subarg->passphrase, subarg->no_passphrase);
      } else {
        CFG_POST_ERR(
            "BITGEN: gen_private_pem:: output should be in .pem extension");
        status = 1;
      }
    } else if (arg->get_sub_arg_name() == "gen_public_pem") {
      const CFGArg_BITGEN_GEN_PUBLIC_PEM* subarg =
          static_cast<const CFGArg_BITGEN_GEN_PUBLIC_PEM*>(arg->get_sub_arg());
      if (CFG_check_file_extensions(subarg->m_args[0], {".pem"}) == 0 &&
          CFG_check_file_extensions(subarg->m_args[1], {".pem"}) == 0) {
        CFGOpenSSL::gen_public_pem(subarg->m_args[0], subarg->m_args[1],
                                   subarg->passphrase);
      } else {
        CFG_POST_ERR(
            "BITGEN: gen_public_pem:: both input and output should be in .pem "
            "extension");
        status = 1;
      }
    } else if (arg->get_sub_arg_name() == "combine_bop") {
      const CFGArg_BITGEN_COMBINE_BOP* subarg =
          static_cast<const CFGArg_BITGEN_COMBINE_BOP*>(arg->get_sub_arg());
      if (CFG_check_file_extensions(subarg->m_args[0], {".cfgbit"}) == 0 &&
          CFG_check_file_extensions(subarg->m_args[1], {".cfgbit"}) == 0 &&
          CFG_check_file_extensions(subarg->m_args[2], {".cfgbit"}) == 0) {
        BitGen_ANALYZER::combine_bitstreams(
            subarg->m_args[0], subarg->m_args[1], subarg->m_args[2]);
      } else {
        CFG_POST_ERR(
            "BITGEN: gen_public_pem:: both input and output should be in "
            ".cfgbit "
            "extension");
        status = 1;
      }
    } else if (arg->get_sub_arg_name() == "gen_ubi") {
      const CFGArg_BITGEN_GEN_UBI* subarg =
          static_cast<const CFGArg_BITGEN_GEN_UBI*>(arg->get_sub_arg());
      std::vector<std::string> inputs;
      std::string output;
      size_t index = 0;
      for (auto& filepath : subarg->m_args) {
        if (index == (subarg->m_args.size() - 1)) {
          output = filepath;
        } else {
          inputs.push_back(filepath);
        }
        index++;
      }
      if (CFG_check_file_extensions(output, {".cfgbit"}) == 0) {
        BitGen_UBI_HEADER header;
        header.header_version = (uint16_t)(subarg->header_version);
        header.product_id = (uint32_t)(subarg->product_id);
        header.customer_id = (uint32_t)(subarg->customer_id);
        header.image_version = (uint32_t)(subarg->image_version);
        header.image_type = (uint8_t)(subarg->type[0]);
        BitGen_UBI::package(header, inputs, output);
        ;
      } else {
        CFG_POST_ERR(
            "BITGEN: gen_public_pem:: output should be in .cfgbit extension");
        status = 1;
      }
    } else {
      CFG_POST_ERR("BITGEN: Invalid sub command %s",
                   arg->get_sub_arg_name().c_str());
      status = 1;
    }
  }
  return status;
}
