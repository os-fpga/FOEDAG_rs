#include "Ocla.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaIP.h"
#include "OclaJtagAdapter.h"
#include "OclaWaveformWriter.h"

OclaDebugSession Ocla::m_session{};

Ocla::Ocla(OclaJtagAdapter *adapter, OclaWaveformWriter *writer)
    : m_adapter(adapter), m_writer(writer) {}

void Ocla::configure(uint32_t domain, std::string mode, std::string condition,
                     uint32_t sample_size) {
  CFG_ASSERT(m_adapter != nullptr);

  if (!m_session.is_loaded()) {
    CFG_POST_ERR("Debug session is not loaded.");
    return;
  }

  OclaDomain *target = nullptr;

  for (auto &elem : m_session.get_clock_domains()) {
    if (elem.get_index() == domain) {
      target = &elem;
    }
  }

  if (!target) {
    CFG_POST_ERR("Domain %d not found", domain);
    return;
  }

  // todo: mem depth check for sampling size

  ocla_config cfg;

  // std::transform(condition.begin(), condition.end(), condition.begin(),
  //                ::toupper);
  cfg.enable_fix_sample_size = sample_size > 0 ? true : false;
  cfg.sample_size = sample_size;
  cfg.mode = convert_ocla_trigger_mode(mode);
  cfg.condition = convert_trigger_condition(CFG_toupper(condition));

  target->set_config(cfg);

  // todo: configure ip
}

void Ocla::add_trigger(uint32_t domain, uint32_t probe, std::string signal,
                       std::string type, std::string event, uint32_t value,
                       uint32_t compare_width) {
  CFG_ASSERT(m_adapter != nullptr);

  if (!m_session.is_loaded()) {
    CFG_POST_MSG("Debug session is not loaded.");
    return;
  }

  // sanity check for trigger type and event pair
  if (!CFG_type_event_sanity_check(type, event)) {
    CFG_POST_ERR("Invalid '%s' event for '%s' trigger", event.c_str(),
                 type.c_str());
    return;
  }

  OclaDomain *target = nullptr;

  for (auto &elem : m_session.get_clock_domains()) {
    if (elem.get_index() == domain) {
      target = &elem;
    }
  }

  if (!target) {
    CFG_POST_ERR("clock domain %d not found", domain);
    return;
  }

  OclaProbe *target_probe = nullptr;

  for (auto &elem : target->get_probes()) {
    if (elem.get_index() == probe) {
      target_probe = &elem;
    }
  }

  if (!target_probe) {
    CFG_POST_ERR("Probe %d not found", probe);
    return;
  }

  bool status = false;
  uint32_t signal_index =
      (uint32_t)CFG_convert_string_to_u64(signal, false, &status);
  OclaSignal *target_signal = nullptr;

  for (auto &elem : target_probe->get_signals()) {
    if (status) {
      if (elem.get_index() == signal_index) {
        target_signal = &elem;
      }
    } else {
      if (elem.get_name() == signal) {
        target_signal = &elem;
      }
    }
  }

  if (!target_signal) {
    CFG_POST_ERR("Signal '%s' not found", signal.c_str());
    return;
  }

  // todo: verify hardware trigger slot

  // add trigger to debug session
  oc_trigger_t trig{};

  trig.instance_index = target_probe->get_instance_index();
  trig.signal = "Probe " + std::to_string(probe) + "/" +
                target_signal->get_name() + " (#" +
                std::to_string(target_signal->get_index()) + ")";
  trig.cfg.probe_num = target_signal->get_pos();
  trig.cfg.type = convert_trigger_type(type);
  trig.cfg.event = convert_trigger_event(event);
  trig.cfg.value = value;
  trig.cfg.compare_width = compare_width;

  target->add_trigger(trig);

  // todo: configure ip
}

#if 0
bool Ocla::start(uint32_t instance, uint32_t timeout,
                 std::string output_filepath) {
  uint32_t countdown = timeout;
  auto ocla_ip = get_ocla_instance(instance);
  ocla_ip.start();

  while (true) {
    // wait for 1 sec
    CFG_sleep_ms(1000);

    if (ocla_ip.get_status() == DATA_AVAILABLE) {
      ocla_data data = ocla_ip.get_data();
      m_writer->set_width(data.width);
      m_writer->set_depth(data.depth);
      if (m_session->is_loaded()) {
        m_writer->set_signals(generate_signal_descriptor(
            get_probe_info(ocla_ip.get_base_addr())));
      } else {
        m_writer->set_signals(generate_signal_descriptor(data.width));
      }
      m_writer->write(data.values, output_filepath);
      break;
    }

    if (timeout > 0) {
      if (countdown == 0) {
        CFG_POST_ERR("Timeout error");
        return false;
      }
      --countdown;
    }
  }
  return true;
}
#endif

void Ocla::show_info() {
  CFG_ASSERT(m_adapter != nullptr);

  if (!m_session.is_loaded()) {
    CFG_POST_MSG("Debug session is not loaded.");
    return;
  }

  CFG_POST_MSG("User design loaded: %s", m_session.get_filepath().c_str());

  for (auto &domain : m_session.get_clock_domains()) {
    CFG_POST_MSG("Clock Domain %d:", domain.get_index());
    for (auto &probe : domain.get_probes()) {
      CFG_POST_MSG("  Probe %d", probe.get_index());
      for (auto &sig : probe.get_signals()) {
        CFG_POST_MSG("    #%d: %s", sig.get_index(), sig.get_name().c_str());
      }
    }

    ocla_config cfg = domain.get_config();

    CFG_POST_MSG("  OCLA Configuration");
    CFG_POST_MSG("    Mode: %s",
                 convert_ocla_trigger_mode_to_string(cfg.mode).c_str());
    CFG_POST_MSG("    Trigger Condition: %s",
                 convert_trigger_condition_to_string(cfg.condition).c_str());
    CFG_POST_MSG("    Enable Fixed Sample Size: %s",
                 cfg.enable_fix_sample_size ? "TRUE" : "FALSE");
    // todo: show mem. depth when fns is disabled
    CFG_POST_MSG("    Sample Size: %d", cfg.sample_size);

    auto triggers = domain.get_triggers();

    CFG_POST_MSG("  OCLA Trigger Configuration");
    if (triggers.size() > 0) {
      uint32_t i = 1;
      for (auto &trig : triggers) {
        switch (trig.cfg.type) {
          case EDGE:
          case LEVEL:
            CFG_POST_MSG(
                "    #%d: signal=%s; mode=%s_%s", i, trig.signal.c_str(),
                convert_trigger_event_to_string(trig.cfg.event).c_str(),
                convert_trigger_type_to_string(trig.cfg.type).c_str());
            break;
          case VALUE_COMPARE:
            CFG_POST_MSG(
                "    #%d: signal=%s; mode=%s; compare_operator=%s; "
                "compare_value=0x%x; compare_width=%d",
                i, trig.signal.c_str(),
                convert_trigger_type_to_string(trig.cfg.type).c_str(),
                convert_trigger_event_to_string(trig.cfg.event).c_str(),
                trig.cfg.value, trig.cfg.compare_width);
            break;
          case TRIGGER_NONE:
            break;
        }
        ++i;
      }
    } else {
      CFG_POST_MSG("    (n/a)");
    }

    CFG_POST_MSG(" ");
  }
}

void Ocla::show_instance_info() {
  CFG_ASSERT(m_adapter != nullptr);

  if (!m_session.is_loaded()) {
    CFG_POST_MSG("Debug session is not loaded.");
    return;
  }

  CFG_POST_MSG("User design loaded   : %s", m_session.get_filepath().c_str());

  for (auto const &instance : m_session.get_instances()) {
    OclaIP ocla_ip{m_adapter, instance.get_baseaddr()};

    CFG_POST_MSG("OCLA %d", instance.get_index() + 1);
    CFG_POST_MSG("  Base address       : 0x%08x", instance.get_baseaddr());
    CFG_POST_MSG("  ID                 : 0x%08x", ocla_ip.get_id());
    CFG_POST_MSG("  Type               : '%s'", ocla_ip.get_type().c_str());
    CFG_POST_MSG("  Version            : 0x%08x", ocla_ip.get_version());
    CFG_POST_MSG("  No. of probes      : %d", ocla_ip.get_number_of_probes());
    CFG_POST_MSG("  Memory depth       : %d", ocla_ip.get_memory_depth());
    CFG_POST_MSG("  DA status          : %d", ocla_ip.get_status());

    auto cfg = ocla_ip.get_config();
    CFG_POST_MSG("  No. of samples     : %d",
                 (cfg.enable_fix_sample_size ? cfg.sample_size
                                             : ocla_ip.get_memory_depth()));
    CFG_POST_MSG("  Trigger mode       : %s",
                 convert_ocla_trigger_mode_to_string(cfg.mode).c_str());
    CFG_POST_MSG("  Trigger condition  : %s",
                 convert_trigger_condition_to_string(cfg.condition).c_str());
    CFG_POST_MSG("  Trigger");

    for (uint32_t ch = 0; ch < ocla_ip.get_trigger_count(); ch++) {
      auto trig_cfg = ocla_ip.get_channel_config(ch);
      switch (trig_cfg.type) {
        case EDGE:
        case LEVEL:
          CFG_POST_MSG("    Channel %d        : probe=%d; mode=%s_%s", ch + 1,
                       trig_cfg.probe_num,
                       convert_trigger_event_to_string(trig_cfg.event).c_str(),
                       convert_trigger_type_to_string(trig_cfg.type).c_str());
          break;
        case VALUE_COMPARE:
          CFG_POST_MSG(
              "    Channel %d        : probe=%d; mode=%s; compare_operator=%s; "
              "compare_value=0x%x; compare_width=%d",
              ch + 1, trig_cfg.probe_num,
              convert_trigger_type_to_string(trig_cfg.type).c_str(),
              convert_trigger_event_to_string(trig_cfg.event).c_str(),
              trig_cfg.value, trig_cfg.compare_width);
          break;
        case TRIGGER_NONE:
          CFG_POST_MSG("    Channel %d        : %s", ch + 1,
                       convert_trigger_type_to_string(trig_cfg.type).c_str());
          break;
      }
    }

    auto probes = m_session.get_probes(instance.get_index());
    if (probes.size() > 0) {
      // print signal table
      CFG_POST_MSG("  Signal Table");
      CFG_POST_MSG(
          "  "
          "+-------+-------------------------------------+--------------+------"
          "--------+");
      CFG_POST_MSG(
          "  | Index | Signal name                         | Bit pos      | "
          "Bitwidth     |");
      CFG_POST_MSG(
          "  "
          "+-------+-------------------------------------+--------------+------"
          "--------+");

      for (auto &probe : probes) {
        for (auto &sig : probe.get_signals()) {
          CFG_POST_MSG("  | %5d | %-35s | %-12d | %-12d |", sig.get_index(),
                       sig.get_name().c_str(), sig.get_pos(),
                       sig.get_bitwidth());
        }
      }

      CFG_POST_MSG(
          "  "
          "+-------+-------------------------------------+--------------+------"
          "--------+");
    }

    CFG_POST_MSG(" ");
  }
}

uint32_t Ocla::show_status(uint32_t instance) {
  CFG_ASSERT(m_adapter != nullptr);
  // OclaIP ocla_ip = get_ocla_instance(instance);
  // std::ostringstream ss;
  // ss << (uint32_t)ocla_ip.get_status();
  // return ss.str();
  return 0;
}

void Ocla::start_session(std::string filepath) {
  CFG_ASSERT(m_adapter != nullptr);

  if (m_session.is_loaded()) {
    CFG_POST_ERR("Debug session is already loaded");
    return;
  }

  if (!std::filesystem::exists(filepath)) {
    CFG_POST_ERR("File '%s' not found", filepath.c_str());
    return;
  }

  m_session.load(filepath);
  // todo: show loading debug info warning/error
}

void Ocla::stop_session() {
  CFG_ASSERT(m_adapter != nullptr);
  if (!m_session.is_loaded()) {
    CFG_POST_ERR("No debug session is loaded");
    return;
  }
  m_session.unload();
}
