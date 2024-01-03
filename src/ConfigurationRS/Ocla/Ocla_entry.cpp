#include <filesystem>
#include <sstream>

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "Configuration/HardwareManager/HardwareManager.h"
#include "Configuration/HardwareManager/OpenocdAdapter.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "FstWaveformWriter.h"
#include "MemorySession.h"
#include "Ocla.h"
#include "OclaOpenocdAdapter.h"

void Ocla_print(std::string output) {
  std::istringstream ss(output);
  std::string line;
  while (std::getline(ss, line)) {
    CFG_POST_MSG("%s", line.c_str());
  }
}

void Ocla_launch_gtkwave(std::string filepath, std::filesystem::path binpath) {
  CFG_ASSERT_MSG(std::filesystem::exists(filepath), "File not found %s",
                 filepath.c_str());
  auto exepath = binpath / "gtkwave" / "bin" / "gtkwave";
  auto cmd = exepath.string() + " " + filepath;
  CFG_compiler_execute_cmd(cmd);
}

void Ocla_entry(CFGCommon_ARG* cmdarg) {
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }

  // setup hardware manager and ocla depencencies
  OclaOpenocdAdapter adapter{cmdarg->toolPath.string()};
  MemorySession session{};
  FstWaveformWriter writer{};
  Ocla ocla{&adapter, &session, &writer};
  FOEDAG::HardwareManager hardware_manager{&adapter};

  // dispatch commands
  std::string subcmd = arg->get_sub_arg_name();
  if (subcmd == "info") {
    auto parms = static_cast<const CFGArg_DEBUGGER_INFO*>(arg->get_sub_arg());
    std::vector<FOEDAG::Tap> taplist{};
    FOEDAG::Device device{};
    if (!hardware_manager.find_device(parms->cable, parms->device, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", parms->device,
                   parms->cable.c_str());
      return;
    }
    adapter.set_target_device(device, taplist);
    Ocla_print(ocla.show_info());
  } else if (subcmd == "load") {
    auto parms = static_cast<const CFGArg_DEBUGGER_LOAD*>(arg->get_sub_arg());
    ocla.start_session(parms->file);
  } else if (subcmd == "unload") {
    ocla.stop_session();
  } else if (subcmd == "config") {
    auto parms = static_cast<const CFGArg_DEBUGGER_CONFIG*>(arg->get_sub_arg());
    if (parms->instance == 0 || parms->instance > 2) {
      CFG_POST_ERR(
          "Invalid instance parameter. Instance should be either 1 or 2.");
      return;
    }
    std::vector<FOEDAG::Tap> taplist{};
    FOEDAG::Device device{};
    if (!hardware_manager.find_device(parms->cable, parms->device, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", parms->device,
                   parms->cable.c_str());
      return;
    }
    adapter.set_target_device(device, taplist);
    ocla.configure(parms->instance, parms->mode, parms->trigger_condition,
                   parms->sample_size);
  } else if (subcmd == "config_channel") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_CONFIG_CHANNEL*>(arg->get_sub_arg());
    if (parms->instance == 0 || parms->instance > 2) {
      CFG_POST_ERR(
          "Invalid instance parameter. Instance should be either 1 or 2.");
      return;
    }
    if (parms->channel == 0 || parms->channel > 4) {
      CFG_POST_ERR("Invalid channel parameter. Channel should be 1 to 4.");
      return;
    }
    std::vector<FOEDAG::Tap> taplist{};
    FOEDAG::Device device{};
    if (!hardware_manager.find_device(parms->cable, parms->device, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", parms->device,
                   parms->cable.c_str());
      return;
    }
    adapter.set_target_device(device, taplist);
    ocla.configure_channel(parms->instance, parms->channel, parms->type,
                           parms->event, parms->value, parms->probe);
  } else if (subcmd == "start") {
    auto parms = static_cast<const CFGArg_DEBUGGER_START*>(arg->get_sub_arg());
    if (parms->instance == 0 || parms->instance > 2) {
      CFG_POST_ERR(
          "Invalid instance parameter. Instance should be either 1 or 2.");
      return;
    }
    std::vector<FOEDAG::Tap> taplist{};
    FOEDAG::Device device{};
    if (!hardware_manager.find_device(parms->cable, parms->device, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", parms->device,
                   parms->cable.c_str());
      return;
    }
    adapter.set_target_device(device, taplist);
    ocla.start(parms->instance, parms->timeout, parms->output);
    CFG_POST_MSG("Written %s successfully.", parms->output.c_str());
    Ocla_launch_gtkwave(parms->output, cmdarg->binPath);
  } else if (subcmd == "status") {
    auto parms = static_cast<const CFGArg_DEBUGGER_STATUS*>(arg->get_sub_arg());
    if (parms->instance == 0 || parms->instance > 2) {
      CFG_POST_ERR(
          "Invalid instance parameter. Instance should be either 1 or 2.");
      return;
    }
    std::vector<FOEDAG::Tap> taplist{};
    FOEDAG::Device device{};
    if (!hardware_manager.find_device(parms->cable, parms->device, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", parms->device,
                   parms->cable.c_str());
      return;
    }
    adapter.set_target_device(device, taplist);
    auto output = ocla.show_status(parms->instance);
    cmdarg->tclOutput = output.c_str();
  } else if (subcmd == "show_waveform") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_WAVEFORM*>(arg->get_sub_arg());
    Ocla_launch_gtkwave(parms->input, cmdarg->binPath);
  } else if (subcmd == "debug") {
    auto parms = static_cast<const CFGArg_DEBUGGER_DEBUG*>(arg->get_sub_arg());
    if (parms->instance == 0 || parms->instance > 2) {
      CFG_POST_ERR(
          "Invalid instance parameter. Instance should be either 1 or 2.");
      return;
    }
    std::vector<FOEDAG::Tap> taplist{};
    FOEDAG::Device device{};
    if (!hardware_manager.find_device(parms->cable, parms->device, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", parms->device,
                   parms->cable.c_str());
      return;
    }
    adapter.set_target_device(device, taplist);
    if (parms->start) ocla.debug_start(parms->instance);
    if (parms->reg) Ocla_print(ocla.dump_registers(parms->instance));
    if (parms->dump || parms->waveform)
      Ocla_print(
          ocla.dump_samples(parms->instance, parms->dump, parms->waveform));
    if (parms->session_info) Ocla_print(ocla.show_session_info());
  } else if (subcmd == "list_cable") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_LIST_CABLE*>(arg->get_sub_arg());

    auto cables = hardware_manager.get_cables();
    if (cables.empty()) {
      if (parms->verbose) CFG_POST_MSG("No cable detected");
    } else {
      if (parms->verbose) {
        CFG_POST_MSG("Cable");
        CFG_POST_MSG("-----------------");
      }
      for (const auto& cable : cables) {
        if (parms->verbose)
          CFG_POST_MSG("(%u) %s", cable.index, cable.name.c_str());
        cmdarg->tclOutput += cable.name + " ";
      }
    }

  } else if (subcmd == "list_device") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_LIST_DEVICE*>(arg->get_sub_arg());
    std::vector<FOEDAG::Device> devices{};

    if (parms->m_args.size() == 1) {
      std::string cable_name = parms->m_args[0];
      if (!hardware_manager.is_cable_exists(cable_name, true)) {
        CFG_POST_ERR("Cable '%s' not found", cable_name.c_str());
        return;
      }
      devices = hardware_manager.get_devices(cable_name, true);
    } else {
      // fild all ocla devices
      devices = hardware_manager.get_devices();
    }

    if (devices.empty()) {
      CFG_POST_MSG("No OCLA device detected");
      return;
    }

    if (parms->verbose) {
      CFG_POST_MSG("Cable                       | Device            ");
      CFG_POST_MSG("------------------------------------------------");
    }

    for (const auto& device : devices) {
      if (parms->verbose)
        CFG_POST_MSG("(%u) %-24s  %s<%u>", device.cable.index,
                     device.cable.name.c_str(), device.name.c_str(),
                     device.index);
      cmdarg->tclOutput += device.cable.name + " " + device.name + "<" +
                           std::to_string(device.index) + "> ";
    }

  } else if (subcmd == "counter") {
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
