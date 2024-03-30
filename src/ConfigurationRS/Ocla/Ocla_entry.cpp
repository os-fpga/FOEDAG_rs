#include <filesystem>
#include <sstream>

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "Configuration/HardwareManager/HardwareManager.h"
#include "Configuration/HardwareManager/OpenocdAdapter.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "Ocla.h"
#include "OclaFstWaveformWriter.h"
#include "OclaDebugSession.h"
#include "OclaOpenocdAdapter.h"

bool Ocla_select_device(OclaJtagAdapter &adapter, FOEDAG::HardwareManager &hardware_manager, 
      std::string cable_name, uint32_t device_index) {
  std::vector<FOEDAG::Tap> taplist{};
  FOEDAG::Device device{};
  if (!hardware_manager.find_device(cable_name, device_index, device,
                                      taplist, true)) {
      CFG_POST_ERR("Could't find device %u on cable '%s'", device_index,
                   cable_name.c_str());
      return false;
  }
    adapter.set_target_device(device, taplist);
    return true;
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
  OclaFstWaveformWriter writer{};
  Ocla ocla{&adapter, &writer};
  FOEDAG::HardwareManager hardware_manager{&adapter};

  // dispatch commands
  std::string subcmd = arg->get_sub_arg_name();
  if (subcmd == "info") {
    ocla.show_info();
  } else if (subcmd == "load") {
    auto parms = static_cast<const CFGArg_DEBUGGER_LOAD*>(arg->get_sub_arg());
    ocla.start_session(parms->file);
  } else if (subcmd == "unload") {
    ocla.stop_session();
  } else if (subcmd == "config") {
    auto parms = static_cast<const CFGArg_DEBUGGER_CONFIG*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      ocla.configure(parms->domain, parms->mode, parms->trigger_condition, parms->sample_size);
    }
  } else if (subcmd == "add_trigger") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_ADD_TRIGGER*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      ocla.add_trigger(parms->domain, parms->probe, parms->signal, parms->type, parms->event, 
        parms->value, parms->compare_width);
    }
  } else if (subcmd == "start") {
    auto parms = static_cast<const CFGArg_DEBUGGER_START*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      // if (ocla.start(parms->instance, parms->timeout, parms->output)) {
      //   CFG_POST_MSG("Written %s successfully.", parms->output.c_str());
      //   Ocla_launch_gtkwave(parms->output, cmdarg->binPath);
      // }
    }
  } else if (subcmd == "status") {
    auto parms = static_cast<const CFGArg_DEBUGGER_STATUS*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      cmdarg->tclOutput = std::to_string(ocla.show_status(parms->domain));
    }
  } else if (subcmd == "show_waveform") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_WAVEFORM*>(arg->get_sub_arg());
    Ocla_launch_gtkwave(parms->input, cmdarg->binPath);
  } else if (subcmd == "show_instance") {
    auto parms = static_cast<const CFGArg_DEBUGGER_SHOW_INSTANCE*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      ocla.show_instance_info();
    }
  } else if (subcmd == "read") {
    auto parms = static_cast<const CFGArg_DEBUGGER_READ*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      auto result = adapter.read((uint32_t)parms->addr, (uint32_t)parms->times,
                                (uint32_t)parms->incr);
      for (auto value : result) {
        CFG_POST_MSG("0x%08x 0x%08x 0x%x", value.address, value.data,
                    value.status);
      }
    }
  } else if (subcmd == "write") {
    auto parms = static_cast<const CFGArg_DEBUGGER_WRITE*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable, parms->device)) {
      adapter.write((uint32_t)parms->addr, (uint32_t)parms->value);
    }
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
  }
}
