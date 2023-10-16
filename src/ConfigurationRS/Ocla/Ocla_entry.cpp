#include <filesystem>
#include <sstream>

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "FstWaveformWriter.h"
#include "MemorySession.h"
#include "Ocla.h"
#include "OpenocdJtagAdapter.h"

void Ocla_print(std::string output) {
  std::istringstream ss(output);
  std::string line;
  while (std::getline(ss, line)) {
    CFG_POST_MSG("%s", line.c_str());
  }
}

void Ocla_launch_gtkwave(std::string filepath, std::filesystem::path binPath) {
  CFG_ASSERT_MSG(std::filesystem::exists(filepath), "File not found %s",
                 filepath.c_str());
  auto exePath = binPath / "gtkwave" / "bin" / "gtkwave";
  auto cmd = exePath.string() + " " + filepath;
  CFG_compiler_execute_cmd(cmd);
}

void Ocla_entry(CFGCommon_ARG* cmdarg) {
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }

  // initialize dependencies
  OpenocdJtagAdapter adapter{cmdarg->toolPath.string(), CFG_execute_cmd};
  MemorySession session{};
  FstWaveformWriter writer{};

  // dispatch commands
  std::string subCmd = arg->get_sub_arg_name();
  if (subCmd == "info") {
    Ocla ocla{&adapter, &session};
    Ocla_print(ocla.showInfo());
  } else if (subCmd == "load") {
    auto parms = static_cast<const CFGArg_DEBUGGER_LOAD*>(arg->get_sub_arg());
    Ocla ocla{&adapter, &session};
    ocla.startSession(parms->file);
  } else if (subCmd == "unload") {
    Ocla ocla{&adapter, &session};
    ocla.stopSession();
  } else if (subCmd == "config") {
    auto parms = static_cast<const CFGArg_DEBUGGER_CONFIG*>(arg->get_sub_arg());
    CFG_ASSERT_MSG(
        parms->instance >= 1 && parms->instance <= 2,
        "Invalid instance parameter. Instance should be either 1 or 2.");
    Ocla ocla{&adapter, &session};
    ocla.configure(parms->instance, parms->mode, parms->trigger_condition,
                   parms->sample_size);
  } else if (subCmd == "config_channel") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_CONFIG_CHANNEL*>(arg->get_sub_arg());
    CFG_ASSERT_MSG(
        parms->instance >= 1 && parms->instance <= 2,
        "Invalid instance parameter. Instance should be either 1 or 2.");
    CFG_ASSERT_MSG(parms->channel >= 1 && parms->channel <= 4,
                   "Invalid channel parameter. Channel should be 1 to 4.");
    Ocla ocla{&adapter, &session};
    ocla.configureChannel(parms->instance, parms->channel, parms->type,
                          parms->event, parms->value, parms->probe);
  } else if (subCmd == "start") {
    auto parms = static_cast<const CFGArg_DEBUGGER_START*>(arg->get_sub_arg());
    CFG_ASSERT_MSG(
        parms->instance >= 1 && parms->instance <= 2,
        "Invalid instance parameter. Instance should be either 1 or 2.");
    Ocla ocla{&adapter, &session, &writer};
    ocla.start(parms->instance, parms->timeout, parms->output);
    CFG_POST_MSG("Written %s successfully.", parms->output.c_str());
    Ocla_launch_gtkwave(parms->output, cmdarg->binPath);
  } else if (subCmd == "status") {
    auto parms = static_cast<const CFGArg_DEBUGGER_STATUS*>(arg->get_sub_arg());
    CFG_ASSERT_MSG(
        parms->instance >= 1 && parms->instance <= 2,
        "Invalid instance parameter. Instance should be either 1 or 2.");
    Ocla ocla{&adapter, &session};
    auto output = ocla.showStatus(parms->instance);
    cmdarg->tclOutput = output.c_str();
  } else if (subCmd == "show_waveform") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_WAVEFORM*>(arg->get_sub_arg());
    Ocla_launch_gtkwave(parms->input, cmdarg->binPath);
  } else if (subCmd == "debug") {
    auto parms = static_cast<const CFGArg_DEBUGGER_DEBUG*>(arg->get_sub_arg());
    CFG_ASSERT_MSG(
        parms->instance >= 1 && parms->instance <= 2,
        "Invalid instance parameter. Instance should be either 1 or 2.");
    Ocla ocla{&adapter, &session, &writer};
    if (parms->start) ocla.debugStart(parms->instance);
    if (parms->reg) Ocla_print(ocla.dumpRegisters(parms->instance));
    if (parms->dump || parms->waveform)
      Ocla_print(
          ocla.dumpSamples(parms->instance, parms->dump, parms->waveform));
    if (parms->session_info) Ocla_print(ocla.showSessionInfo());
  } else if (subCmd == "counter") {
    // for testing with IP on ocla platform only.
    // Will be removed at final
    auto parms =
        static_cast<const CFGArg_DEBUGGER_COUNTER*>(arg->get_sub_arg());
    if (parms->start ^ parms->stop) {
      adapter.write(0x01000004, parms->start ? 0xffffffff : 0x0);
    }
    if (parms->reset) {
      adapter.write(0x01000000, 0xffffffff);
      adapter.write(0x01000000, 0);
    }
    if (parms->read) {
      CFG_POST_MSG("counter1 = 0x%08x", adapter.read(0x01000008));
      CFG_POST_MSG("counter2 = 0x%08x", adapter.read(0x0100000c));
    }
  }
}
