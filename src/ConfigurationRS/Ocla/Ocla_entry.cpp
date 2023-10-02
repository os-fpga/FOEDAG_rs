#include <filesystem>

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "Ocla.h"
#include "OclaException.h"
#include "OclaIP.h"
#include "OpenocdJtagAdapter.h"

void Ocla_print(std::stringstream& output) {
  std::string s;
  while (std::getline(output, s)) {
    CFG_POST_MSG("%s", s.c_str());
  }
}

void Ocla_launch_gtkwave(std::string waveFile, std::filesystem::path binPath) {
  std::error_code ec;
  if (std::filesystem::exists(waveFile, ec)) {
    auto exePath = binPath / "gtkwave" / "bin" / "gtkwave";
    auto cmd = exePath.string() + " " + waveFile;
    if (system(cmd.c_str())) {
      CFG_POST_ERR("Fail to open GTKWave");
    }
  } else {
    CFG_POST_ERR("File not found: %s", waveFile.c_str());
  }
}

void Ocla_entry(CFGCommon_ARG* cmdarg) {
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }

  std::string subCmd = arg->get_sub_arg_name();
  if (subCmd == "info") {
    try {
      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
      std::stringstream output{};
      Ocla ocla{};
      ocla.setJtagAdapter(&openocd);
      ocla.showInfo(output);
      Ocla_print(output);
    } catch (const OclaException& e) {
      CFG_POST_ERR("%s (%d). %s", e.what(), e.getError(), e.getMessage());
    }
  } else if (subCmd == "session") {
    try {
      auto parms =
          static_cast<const CFGArg_DEBUGGER_SESSION*>(arg->get_sub_arg());
      if (parms->start ^ parms->stop) {
        Ocla ocla{};
        if (parms->start) {
          ocla.startSession(parms->file);
        } else {
          ocla.stopSession();
        }
      }
    } catch (const OclaException& e) {
      CFG_POST_ERR("%s (%d). %s", e.what(), e.getError(), e.getMessage());
    }
  } else if (subCmd == "config") {
    try {
      auto parms =
          static_cast<const CFGArg_DEBUGGER_CONFIG*>(arg->get_sub_arg());
      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
      Ocla ocla{};
      ocla.setJtagAdapter(&openocd);
      ocla.configure(parms->instance, parms->mode, parms->trigger_condition,
                     parms->sample_size);
    } catch (const OclaException& e) {
      CFG_POST_ERR("%s (%d). %s", e.what(), e.getError(), e.getMessage());
    }
  } else if (subCmd == "config_channel") {
    try {
      auto parms = static_cast<const CFGArg_DEBUGGER_CONFIG_CHANNEL*>(
          arg->get_sub_arg());
      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
      Ocla ocla{};
      ocla.setJtagAdapter(&openocd);
      ocla.configureChannel(parms->instance, parms->channel, parms->type,
                            parms->event, parms->value, parms->probe);
    } catch (const OclaException& e) {
      CFG_POST_ERR("%s (%d). %s", e.what(), e.getError(), e.getMessage());
    }
  } else if (subCmd == "start") {
    try {
      auto parms =
          static_cast<const CFGArg_DEBUGGER_START*>(arg->get_sub_arg());
      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
      Ocla ocla{};
      ocla.setJtagAdapter(&openocd);
      ocla.start(parms->instance, parms->timeout, parms->output);
      CFG_POST_MSG("Written %s successfully.", parms->output.c_str());
      Ocla_launch_gtkwave(parms->output, cmdarg->binPath);
    } catch (const OclaException& e) {
      CFG_POST_ERR("%s (%d). %s", e.what(), e.getError(), e.getMessage());
    }
  } else if (subCmd == "status") {
    try {
      auto parms =
          static_cast<const CFGArg_DEBUGGER_STATUS*>(arg->get_sub_arg());
      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
      std::stringstream output{};
      Ocla ocla{};
      ocla.setJtagAdapter(&openocd);
      ocla.showStatus(parms->instance, output);
      cmdarg->tclOutput = output.str().c_str();
    } catch (const OclaException& e) {
      CFG_POST_ERR("%s (%d). %s", e.what(), e.getError(), e.getMessage());
    }
  } else if (subCmd == "show_waveform") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_WAVEFORM*>(arg->get_sub_arg());
    Ocla_launch_gtkwave(parms->input, cmdarg->binPath);
  } else if (subCmd == "debug") {
    try {
      auto parms =
          static_cast<const CFGArg_DEBUGGER_DEBUG*>(arg->get_sub_arg());

      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};
      OclaIP ip{&openocd, parms->instance == 1 ? OCLA1_ADDR : OCLA2_ADDR};

      if (parms->start) ip.start();

      if (parms->reg) {
        std::map<uint32_t, std::string> regs = {
            {0x00, "OCSR"},       {0x08, "TCUR0"}, {0x0c, "TMTR"},
            {0x10, "TDCR"},       {0x14, "TCUR1"}, {0x18, "IP_TYPE"},
            {0x1c, "IP_VERSION"}, {0x20, "IP_ID"},
        };

        for (auto const& [offset, name] : regs) {
          uint32_t regaddr = ip.getBaseAddr() + offset;
          CFG_POST_MSG("%-10s (0x%08x) = 0x%08x", name.c_str(), regaddr,
                       openocd.read(regaddr));
        }
      }

      if (parms->dump || parms->waveform) {
        ocla_data data = ip.getData();

        if (parms->dump) {
          CFG_POST_MSG("linewidth      : %d", data.linewidth);
          CFG_POST_MSG("depth          : %d", data.depth);
          CFG_POST_MSG("reads_per_line : %d", data.reads_per_line);
          CFG_POST_MSG("length         : %d", data.values.size());
          for (auto& v : data.values) {
            CFG_POST_MSG("0x%08x", v);
          }
        }
      }
    } catch (const std::exception& e) {
      CFG_POST_ERR(e.what());
    }
  } else if (subCmd == "counter") {
    try {
      // for testing with IP on ocla platform only.
      // Will be removed at final
      auto parms =
          static_cast<const CFGArg_DEBUGGER_COUNTER*>(arg->get_sub_arg());

      OpenocdJtagAdapter openocd{cmdarg->toolPath, CFG_execute_cmd};

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
    } catch (const std::exception& e) {
      CFG_POST_ERR(e.what());
    }
  }
}
