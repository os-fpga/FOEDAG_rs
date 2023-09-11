#include "Ocla.h"

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"

void Ocla_entry(const CFGCommon_ARG* cmdarg) {
  CFG_POST_MSG("This is OCLA entry");
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }

  std::string subCmd = arg->get_sub_arg_name();
  if (subCmd == "info") {
    CFG_POST_MSG("<implement info tcl command here>");
  } else if (subCmd == "session") {
    CFG_POST_MSG("<implement session start/stop tcl command here>");
  } else if (subCmd == "config") {
    CFG_POST_MSG("<implement config tcl command here>");
  } else if (subCmd == "config_channel") {
    CFG_POST_MSG("<implement config_channel tcl command here>");
  }
}
