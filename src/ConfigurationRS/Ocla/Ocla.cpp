#include "Ocla.h"

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"

void Ocla_entry(CFGCommon_ARG* cmdarg) {
  CFG_POST_MSG("This is OCLA entry");
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }
  CFG_POST_MSG("tool path = %s", cmdarg->toolPath.c_str());
  std::string subCmd = arg->get_sub_arg_name();
  if (subCmd == "info") {
    CFG_POST_MSG("<implement info tcl command here>");
  } else if (subCmd == "session") {
    CFG_POST_MSG("<implement session start/stop tcl command here>");
  } else if (subCmd == "config") {
    CFG_POST_MSG("<implement config tcl command here>");
  } else if (subCmd == "config_channel") {
    CFG_POST_MSG("<implement config_channel tcl command here>");
  } else if (subCmd == "start") {
    CFG_POST_MSG("<implement start tcl command here>");
  } else if (subCmd == "status") {
    CFG_POST_MSG("<implement status tcl command here>");
  } else if (subCmd == "show_waveform") {
    CFG_POST_MSG("<implement show_waveform tcl command here>");
  }
}
