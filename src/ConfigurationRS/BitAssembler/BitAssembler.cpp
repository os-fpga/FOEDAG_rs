#include "BitAssembler.h"

#include "BitAssembler_ddb.h"
#include "BitAssembler_mgr.h"
#include "BitAssembler_ocla.h"
#include "BitGenerator/BitGenerator.h"
#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "Utils/FileUtils.h"

void BitAssembler_entry(CFGCommon_ARG* cmdarg) {
  CFG_TIME time_begin = CFG_time_begin();
  std::string bitasm_time = CFG_get_time();
  CFG_POST_MSG("This is BITASM entry");
  CFG_POST_MSG("  Project: %s", cmdarg->projectName.c_str());
  CFG_POST_MSG("  Device: %s", cmdarg->device.c_str());
  CFG_POST_MSG("  Task Path: %s", cmdarg->taskPath.c_str());
  CFG_POST_MSG("  Search Path: %s", cmdarg->searchPath.c_str());
  CFG_POST_MSG("  Time: %s", bitasm_time.c_str());
  std::string bitasm_file = CFG_print("%s/%s.bitasm", cmdarg->taskPath.c_str(),
                                      cmdarg->projectName.c_str());
  std::string cfgbit_file = CFG_print("%s/%s.cfgbit", cmdarg->taskPath.c_str(),
                                      cmdarg->projectName.c_str());
  CFG_POST_MSG("  Operation: %s", (cmdarg->clean ? "clean" : "generate"));
  bool bitgen = false;
  if (cmdarg->clean) {
    FOEDAG::FileUtils::removeFile(bitasm_file);
    FOEDAG::FileUtils::removeFile(cfgbit_file);
  } else {
    CFG_POST_MSG("  Output: %s", bitasm_file.c_str());
    CFGObject_BITOBJ bitobj;
    bitobj.write_str("version", "Raptor 1.0");
    bitobj.write_str("project", cmdarg->projectName);
    bitobj.write_str("device", cmdarg->device);
    bitobj.write_str("time", bitasm_time);
    // Get Device Database
    BitAssembler_DEVICE device;
    std::string ddb_file =
        CFG_print("%s/devices.ddb", cmdarg->searchPath.c_str());
    CFG_ddb_search_device(ddb_file, cmdarg->device, device);
    if (device.protocol == "scan_chain") {
      CFG_ASSERT(device.blwl.size() == 0);
    } else if (device.protocol == "ql_memory_bank") {
      CFG_ASSERT_MSG(device.blwl == "flatten",
                     "Does not support configuration protocol %s with %s BL/WL",
                     device.protocol.c_str(), device.blwl.c_str());
    } else {
      CFG_INTERNAL_ERROR("Does not support configuration protocol %s",
                         device.protocol.c_str());
    }
    bitobj.configuration.write_str("family", device.family);
    bitobj.configuration.write_str("series", device.series);
    bitobj.configuration.write_str("protocol", device.protocol);
    bitobj.configuration.write_str("blwl", device.blwl);
    // FCB
    BitAssembler_MGR mgr(cmdarg->taskPath, cmdarg->device);
    if (device.protocol == "scan_chain") {
      mgr.get_scan_chain_fcb(&bitobj.scan_chain_fcb);
    } else {
      mgr.get_ql_membank_fcb(&bitobj.ql_membank_fcb);
    }

    // OCLA
    std::string yosysBin = CFG_print("%s/yosys", cmdarg->binPath.c_str());
    std::string hierPath =
        CFG_print("%s/hier_info.json", cmdarg->analyzePath.c_str());
    std::string ysPath = CFG_print("%s/%s.ys", cmdarg->synthesisPath.c_str(),
                                   cmdarg->projectName.c_str());
    BitAssembler_OCLA::parse(bitobj, cmdarg->taskPath.c_str(), yosysBin,
                             hierPath, ysPath);

    // Writing out
    bitgen = bitobj.write(bitasm_file);
    CFG_POST_MSG("  Status: %s", bitgen ? "success" : "fail");
  }
  CFG_POST_MSG("BITASM elapsed time: %.3f seconds",
               CFG_time_elapse(time_begin));
  if (bitgen) {
    CFGCommon_ARG* cmdarg_ptr = const_cast<CFGCommon_ARG*>(cmdarg);
    auto arg = std::make_shared<CFGArg_BITGEN>();
    const char* args[3] = {"gen_bitstream", bitasm_file.c_str(),
                           cfgbit_file.c_str()};
    CFG_ASSERT(arg->parse(3, args));
    cmdarg_ptr->arg = arg;
    BitGenerator_entry(cmdarg_ptr);
  }
}
