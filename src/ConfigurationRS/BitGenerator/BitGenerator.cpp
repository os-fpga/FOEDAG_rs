#include "BitGenerator.h"

#include <map>

#include "BitGen_analyzer.h"
#include "BitGen_gemini.h"
#include "BitGen_json.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

static bool read_aes_key(std::string filepath, std::vector<uint8_t>& aes_key) {
  CFG_ASSERT(aes_key.size() == 0);
  bool status = false;
  if (filepath.size()) {
    CFG_read_binary_file(filepath, aes_key);
    if (aes_key.size() == 16 || aes_key.size() == 32) {
      status = true;
    } else {
      CFG_POST_ERR(
          "BITGEN: AES key should be 16 or 32 Bytes. But found %ld Bytes in %s",
          aes_key.size(), filepath.c_str());
    }
  } else {
    status = true;
  }
  return status;
}

bool BitGenerator_entry(const CFGCommon_ARG* cmdarg) {
  bool status = true;
  CFG_TIME time_begin = CFG_time_begin();
  // Validate arg
  // Make sure correct argument is set before we do the casting
  CFG_ASSERT(cmdarg->arg->m_name == "bitgen");
  auto arg = std::static_pointer_cast<CFGArg_BITGEN>(cmdarg->arg);
  std::vector<uint8_t> aes_key;
  if (arg->get_sub_arg_name() == "gen_bitstream") {
    const CFGArg_BITGEN_GEN_BITSTREAM* subarg =
        static_cast<const CFGArg_BITGEN_GEN_BITSTREAM*>(arg->get_sub_arg());
    CFG_POST_MSG("  Input: %s", subarg->m_args[0].c_str());
    CFG_POST_MSG("  Output: %s", subarg->m_args[1].c_str());
    if (CFG_check_file_extensions(subarg->m_args[0], {".bitasm", ".json"}) <
            0 ||
        CFG_check_file_extensions(subarg->m_args[1], {".cfgbit"}) < 0) {
      CFG_POST_ERR(
          "BITGEN: parse::, input should be in .bitasm or .cfgbit extension, "
          "and output should be in .debug.txt extension");
      status = false;
    }
    status = status && read_aes_key(subarg->aes_key, aes_key);
    if (status) {
      // Signing key
      CFGCrypto_KEY key;
      CFGCrypto_KEY* key_ptr = nullptr;
      if (subarg->signing_key.size()) {
        key.initial(subarg->signing_key, subarg->passphrase, true);
        key_ptr = &key;
      }
      std::vector<BitGen_BITSTREAM_BOP*> bops;
      if (CFG_check_file_extensions(subarg->m_args[0], {".bitasm"}) == 0) {
        // Read the BitObj file
        CFGObject_BITOBJ bitobj;
        CFG_ASSERT(bitobj.read(subarg->m_args[0]));
        // Get the family
        if (bitobj.configuration.family == "Gemini") {
          BitGen_GEMINI gemini(&bitobj);
          gemini.generate(bops);
        } else {
          CFG_INTERNAL_ERROR("Unsupported device %s family %s",
                             bitobj.device.c_str(),
                             bitobj.configuration.family.c_str());
        }
      } else {
        BitGen_JSON::parse_bitstream(subarg->m_args[0], bops);
        CFG_ASSERT(bops.size())
      }
      std::vector<uint8_t> data;
      std::string bitstream_error_msg = "";
      BitGen_PACKER::generate_bitstream(bops, data, subarg->compress, aes_key,
                                        key_ptr);
      BitGen_ANALYZER::parse(data, true, true, bitstream_error_msg, false);
      CFG_ASSERT_MSG(bitstream_error_msg.empty(), bitstream_error_msg.c_str());
      CFG_write_binary_file(subarg->m_args[1], &data[0], data.size());
      memset(&data[0], 0, data.size());
      data.clear();
      while (bops.size()) {
        CFG_MEM_DELETE(bops.back());
        bops.pop_back();
      }
    }
  } else if (arg->get_sub_arg_name() == "parse") {
    const CFGArg_BITGEN_PARSE* subarg =
        static_cast<const CFGArg_BITGEN_PARSE*>(arg->get_sub_arg());
    CFG_POST_MSG("  Input: %s", subarg->m_args[0].c_str());
    CFG_POST_MSG("  Output: %s", subarg->m_args[1].c_str());
    if (CFG_check_file_extensions(subarg->m_args[0], {".bitasm", ".cfgbit"}) <
            0 ||
        CFG_check_file_extensions(subarg->m_args[1], {".debug.txt"}) < 0) {
      CFG_POST_ERR(
          "BITGEN: parse::, input should be in .bitasm or .cfgbit extension, "
          "and output should be in .debug.txt extension");
      status = false;
    }
    status = status && read_aes_key(subarg->aes_key, aes_key);
    if (status) {
      if (CFG_check_file_extensions(subarg->m_args[0], {".bitasm"}) == 0) {
        CFGObject::parse(subarg->m_args[0], subarg->m_args[1], subarg->detail);
      } else {
        BitGen_ANALYZER::parse_debug(subarg->m_args[0], subarg->m_args[1],
                                     aes_key);
      }
    }
  } else {
    CFG_POST_ERR("BITGEN: Invalid sub command %s",
                 arg->get_sub_arg_name().c_str());
    status = false;
  }
  if (aes_key.size()) {
    memset(&aes_key[0], 0, aes_key.size());
    aes_key.clear();
  }
  CFG_POST_MSG("BITGEN elapsed time: %.3f seconds",
               CFG_time_elapse(time_begin));
  return status;
}
