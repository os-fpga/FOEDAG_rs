#include <filesystem>

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "Ocla.h"
#include "OpenocdJtagAdapter.h"

void Ocla_print(std::stringstream& output) {
  std::string s;
  while (std::getline(output, s)) {
    CFG_POST_MSG("%s", s.c_str());
  }
}

void Ocla_launch_gtkwave(std::string filepath, std::filesystem::path binPath) {
  std::error_code ec;
  CFG_ASSERT_MSG(std::filesystem::exists(filepath, ec),
                 (std::string("File not found: ") + filepath).c_str());
  auto exePath = binPath / "gtkwave" / "bin" / "gtkwave";
  auto cmd = exePath.string() + " " + filepath;
  CFG_ASSERT_MSG(system(cmd.c_str()) == 0, "Fail to open GTKWave");
}

void Ocla_entry(CFGCommon_ARG* cmdarg) {
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }

  OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
  std::string subCmd = arg->get_sub_arg_name();
  if (subCmd == "info") {
    Ocla ocla{&openocd};
    auto output = ocla.showInfo();
    Ocla_print(output);
  } else if (subCmd == "session") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SESSION*>(arg->get_sub_arg());
    if (parms->start ^ parms->stop) {
      Ocla ocla{&openocd};
      if (parms->start) {
        ocla.startSession(parms->file);
      } else {
        ocla.stopSession();
      }
    }
  } else if (subCmd == "config") {
    auto parms = static_cast<const CFGArg_DEBUGGER_CONFIG*>(arg->get_sub_arg());
    Ocla ocla{&openocd};
    ocla.configure(parms->instance, parms->mode, parms->trigger_condition,
                   parms->sample_size);
  } else if (subCmd == "config_channel") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_CONFIG_CHANNEL*>(arg->get_sub_arg());
    Ocla ocla{&openocd};
    ocla.configureChannel(parms->instance, parms->channel, parms->type,
                          parms->event, parms->value, parms->probe);
  } else if (subCmd == "start") {
    auto parms = static_cast<const CFGArg_DEBUGGER_START*>(arg->get_sub_arg());
    Ocla ocla{&openocd};
    ocla.start(parms->instance, parms->timeout, parms->output);
    CFG_POST_MSG("Written %s successfully.", parms->output.c_str());
    Ocla_launch_gtkwave(parms->output, cmdarg->binPath);
  } else if (subCmd == "status") {
    auto parms = static_cast<const CFGArg_DEBUGGER_STATUS*>(arg->get_sub_arg());
    Ocla ocla{&openocd};
    auto output = ocla.showStatus(parms->instance);
    cmdarg->tclOutput = output.str().c_str();
  } else if (subCmd == "show_waveform") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_WAVEFORM*>(arg->get_sub_arg());
    Ocla_launch_gtkwave(parms->input, cmdarg->binPath);
  } else if (subCmd == "debug") {
    auto parms = static_cast<const CFGArg_DEBUGGER_DEBUG*>(arg->get_sub_arg());
    Ocla ocla{&openocd};
    if (parms->reg) {
      auto output = ocla.dumpRegisters(parms->instance);
      Ocla_print(output);
    }
  } else if (subCmd == "counter") {
    // for testing with IP on ocla platform only.
    // Will be removed at final
    auto parms =
        static_cast<const CFGArg_DEBUGGER_COUNTER*>(arg->get_sub_arg());
    if (parms->start ^ parms->stop) {
      openocd.write(0x01000004, parms->start ? 0xffffffff : 0x0);
    }
    if (parms->reset) {
      openocd.write(0x01000000, 0xffffffff);
      openocd.write(0x01000000, 0);
    }
    if (parms->read) {
      CFG_POST_MSG("counter1 = 0x%08x", openocd.read(0x01000008));
      CFG_POST_MSG("counter2 = 0x%08x", openocd.read(0x0100000c));
    }
  }
}
