#include "BitGenerator.h"

#include "BitGen_gemini.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

const std::map<std::string, std::vector<std::string>> FAMILY_DATABASE = {
    {"GEMINI", {"1GE75"}}};

static std::string get_device_family(const std::string& device) {
  std::string family = "";
  for (auto& db : FAMILY_DATABASE) {
    if (CFG_find_string_in_vector(db.second, device) != -1) {
      family = db.first;
      break;
    }
  }
  CFG_ASSERT_MSG(family.size(), "Unsupported device %s", device.c_str());
  return family;
}

void BitGenerator_entry(const CFGCommon_ARG* cmdarg) {
  CFG_TIME time_begin = CFG_time_begin();
  CFG_POST_MSG("This is BITGEN entry");

  // Validate arg
  // Make sure correct argument is set before we do the casting
  CFG_ASSERT(cmdarg->arg->m_name == "bitgen");
  CFGArg_BITGEN* arg = reinterpret_cast<CFGArg_BITGEN*>(cmdarg->arg);
  CFG_ASSERT(arg->m_args.size() == 2);
  CFG_POST_MSG("  Input: %s", arg->m_args[0].c_str());
  CFG_POST_MSG("  Output: %s", arg->m_args[1].c_str());

  if (arg->operation == "parse") {
    if (CFG_check_file_extensions(arg->m_args[0], {".bitasm", ".cfgbit"}) < 0 ||
        CFG_check_file_extensions(arg->m_args[1], {".debug.txt"}) != 0) {
      CFG_POST_ERR(
          "BITGEN: For parse operation, input should be in .bitasm or .cfgbit "
          "extension, and output should be in .debug.txt extension");
      return;
    }
  } else {
    // generate
    if (CFG_check_file_extensions(arg->m_args[0], {".bitasm"}) != 0 ||
        CFG_check_file_extensions(arg->m_args[1], {".cfgbit"}) != 0) {
      CFG_POST_ERR(
          "BITGEN: For gen_bitstream operation, input should be in .bitasm "
          "extension, and output should be in .cfgbit extension");
      return;
    }
  }

  if (arg->operation == "parse") {
    if (CFG_check_file_extensions(arg->m_args[0], {".bitasm"}) == 0) {
      CFGObject::parse(arg->m_args[0], arg->m_args[1], arg->detail);
    } else {
      BitGen_GEMINI::parse(arg->m_args[0], arg->m_args[1], arg->detail);
    }
  } else {
    // Read the BitObj file
    CFGObject_BITOBJ bitobj;
    CFG_ASSERT(bitobj.read(arg->m_args[0]));

    // Get the family
    std::vector<uint8_t> data;
    std::string family = get_device_family(bitobj.device);
    if (family == "GEMINI") {
      BitGen_GEMINI gemini(&bitobj);
      CFG_ASSERT(gemini.generate(data));
      CFG_write_binary_file(arg->m_args[1], &data[0], data.size());
    } else {
      CFG_INTERNAL_ERROR("Unsupported device %s family %s",
                         bitobj.device.c_str(), family.c_str());
    }
  }
  CFG_POST_MSG("BITGEN elapsed time: %.3f seconds",
               CFG_time_elapse(time_begin));
}