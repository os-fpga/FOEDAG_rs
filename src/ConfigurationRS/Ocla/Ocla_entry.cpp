#include <filesystem>

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "CFGObject/CFGObject_auto.h"
#include "Configuration/HardwareManager/HardwareManager.h"
#include "Configuration/HardwareManager/OpenocdAdapter.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "Ocla.h"
#include "OclaDebugSession.h"
#include "OclaFstWaveformWriter.h"
#include "OclaOpenocdAdapter.h"

#if _WIN32
#define DEF_FST_OUTPUT "C:\\Windows\\Temp\\output.fst"
#else
#define DEF_FST_OUTPUT "/tmp/output.fst"
#endif

#define OCLA_WAIT_TIME_MS (1000)

bool Ocla_select_device(OclaJtagAdapter& adapter,
                        FOEDAG::HardwareManager& hardware_manager,
                        std::string cable_name, uint32_t device_index) {
  std::vector<FOEDAG::Tap> taplist{};
  FOEDAG::Device device{};
  if (!hardware_manager.find_device(cable_name, device_index, device, taplist,
                                    true)) {
    CFG_POST_ERR("Could't find device %u on cable '%s'", device_index,
                 cable_name.c_str());
    return false;
  }
  adapter.set_target_device(device, taplist);
  return true;
}

void Ocla_launch_gtkwave(oc_waveform_t& waveform, std::filesystem::path binpath,
                         std::string output_filepath) {
  OclaFstWaveformWriter fst_writer{};

  if (fst_writer.write(waveform, output_filepath)) {
    auto exepath = binpath / "gtkwave" / "bin" / "gtkwave";
    auto cmd = exepath.string() + " " + output_filepath;
    CFG_POST_MSG("Output file written at '%s' successfully.",
                 output_filepath.c_str());
    CFG_compiler_execute_cmd(cmd);
  }
}

void Ocla_wait_n_show_waveform(Ocla& ocla, uint32_t domain_id,
                               uint32_t timeout_sec,
                               std::string output_filepath,
                               std::filesystem::path binpath) {
  uint32_t status = 0;

  // query the status of the ocla ip for max of 'timeout_sec' times
  // if status is set, break the loop
  for (uint32_t i = 0; i < timeout_sec; i++) {
    CFG_sleep_ms(OCLA_WAIT_TIME_MS);  // wait for 1 sec
    if (!ocla.get_status(domain_id, status)) {
      CFG_POST_ERR("Failed to read ocla status");
      return;
    }
    if (status) {
      break;
    }
  }

  if (!status) {
    CFG_POST_ERR("Timeout");
    return;
  }

  // download the waveform from ocla ip
  oc_waveform_t output_waveform{};
  if (!ocla.get_waveform(domain_id, output_waveform)) {
    CFG_POST_ERR("Failed to read waveform data");
    return;
  }

  // display the wave from on gtkwave
  Ocla_launch_gtkwave(output_waveform, binpath, output_filepath);
}

void Ocla_entry(CFGCommon_ARG* cmdarg) {
  auto arg = std::static_pointer_cast<CFGArg_DEBUGGER>(cmdarg->arg);
  if (arg == nullptr) return;

  if (arg->m_help) {
    return;
  }

  // setup hardware manager and ocla depencencies
  OclaOpenocdAdapter adapter{cmdarg->toolPath.string()};
  Ocla ocla{&adapter};
  FOEDAG::HardwareManager hardware_manager{&adapter};

  // dispatch commands
  std::string subcmd = arg->get_sub_arg_name();
  if (subcmd == "info") {
    auto parms = static_cast<const CFGArg_DEBUGGER_INFO*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.show_info();
    }
  } else if (subcmd == "load") {
    auto parms = static_cast<const CFGArg_DEBUGGER_LOAD*>(arg->get_sub_arg());
    ocla.start_session(parms->file);
  } else if (subcmd == "unload") {
    ocla.stop_session();
  } else if (subcmd == "config") {
    auto parms = static_cast<const CFGArg_DEBUGGER_CONFIG*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.configure(parms->domain, parms->mode, parms->trigger_condition,
                     parms->sample_size);
    }
  } else if (subcmd == "add_trigger") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_ADD_TRIGGER*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.add_trigger(parms->domain, parms->probe, parms->signal, parms->type,
                       parms->event, parms->value, parms->compare_width);
    }
  } else if (subcmd == "edit_trigger") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_EDIT_TRIGGER*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.edit_trigger(parms->domain, parms->index, parms->probe,
                        parms->signal, parms->type, parms->event, parms->value,
                        parms->compare_width);
    }
  } else if (subcmd == "remove_trigger") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_REMOVE_TRIGGER*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.remove_trigger(parms->domain, parms->index);
    }
  } else if (subcmd == "start") {
    auto parms = static_cast<const CFGArg_DEBUGGER_START*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      if (ocla.start(parms->domain)) {
        if (parms->show_waveform == "true") {
          Ocla_wait_n_show_waveform(
              ocla, parms->domain, (uint32_t)parms->timeout,
              parms->output.empty() ? DEF_FST_OUTPUT : parms->output,
              cmdarg->binPath);
        }
      }
    }
  } else if (subcmd == "status") {
    auto parms = static_cast<const CFGArg_DEBUGGER_STATUS*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      uint32_t status = 0;
      if (ocla.get_status(parms->domain, status)) {
        CFG_POST_MSG("%d - %s", status,
                     (status ? "Data Available" : "Data Not Available"));
        cmdarg->tclOutput = std::to_string(status);
      }
    }
  } else if (subcmd == "show_waveform") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_WAVEFORM*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      oc_waveform_t output_waveform{};
      if (ocla.get_waveform(parms->domain, output_waveform)) {
        Ocla_launch_gtkwave(
            output_waveform, cmdarg->binPath,
            parms->output.empty() ? DEF_FST_OUTPUT : parms->output);
      }
    }
  } else if (subcmd == "show_instance") {
    auto parms =
        static_cast<const CFGArg_DEBUGGER_SHOW_INSTANCE*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.show_instance_info();
    }
  } else if (subcmd == "set_io") {
    auto parms = static_cast<const CFGArg_DEBUGGER_SET_IO*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      ocla.set_io(parms->m_args);
    }
  } else if (subcmd == "get_io") {
    auto parms = static_cast<const CFGArg_DEBUGGER_GET_IO*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      for (uint64_t i = 0; i < parms->loop; i++) {
        std::vector<eio_value_t> values{};
        std::string output{};
        bool first = true;
        if (!ocla.get_io(parms->m_args, values)) {
          break;
        }
        if (values.size() != parms->m_args.size()) {
          CFG_POST_ERR("Result value length mismatched (expect=%d, actual=%d)",
                       parms->m_args.size(), values.size());
          break;
        }
        for (auto& v : values) {
          uint64_t value = v.value[0];
          if (v.value.size() > 1) {
            value |= uint64_t(v.value[1]) << 32;
          }
          if (!first) {
            output += ", " + v.signal_name + " (#" + std::to_string(v.idx) +
                      ") = " + std::to_string(value);
          } else {
            output += v.signal_name + " (#" + std::to_string(v.idx) +
                      ") = " + std::to_string(value);
            first = false;
          }
        }
        CFG_POST_MSG("Iteration %d: %s", i + 1, output.c_str());
        CFG_sleep_ms((uint32_t)parms->interval);
      }
    }
  } else if (subcmd == "read") {
    auto parms = static_cast<const CFGArg_DEBUGGER_READ*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
      auto result = adapter.read((uint32_t)parms->addr, (uint32_t)parms->times,
                                 (uint32_t)parms->incr);
      for (auto value : result) {
        CFG_POST_MSG("0x%08x 0x%08x 0x%x", value.address, value.data,
                     value.status);
      }
    }
  } else if (subcmd == "write") {
    auto parms = static_cast<const CFGArg_DEBUGGER_WRITE*>(arg->get_sub_arg());
    if (Ocla_select_device(adapter, hardware_manager, parms->cable,
                           parms->device)) {
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
