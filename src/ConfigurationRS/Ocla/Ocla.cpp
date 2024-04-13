#include "Ocla.h"

#include <map>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaIP.h"
#include "OclaJtagAdapter.h"

std::vector<OclaDebugSession> Ocla::m_sessions{};

Ocla::Ocla(OclaJtagAdapter *adapter) : m_adapter(adapter) {}

Ocla::~Ocla() {}

void Ocla::configure(uint32_t domain_id, std::string mode,
                     std::string condition, uint32_t sample_size) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;
  OclaInstance *instance = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain)) {
    return;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return;
  }

  for (auto &elem : domain->get_instances()) {
    instance = &elem;
    break;
  }

  if (!instance) {
    CFG_POST_ERR("No instance found for clock domain %d", domain_id);
    return;
  }

  // NOTE:
  // Use the first instance available to check the memory depth since all will
  // have same memory depth in a clock domain.
  // Ensure sampling size is lesser than memory depth
  if (sample_size > instance->get_memory_depth()) {
    CFG_POST_ERR("Sampling size is larger than maximum of %d",
                 instance->get_memory_depth());
    return;
  }

  ocla_config cfg;

  cfg.enable_fix_sample_size = sample_size > 0 ? true : false;
  cfg.sample_size = sample_size;
  cfg.mode = convert_ocla_trigger_mode(mode);
  cfg.condition = convert_trigger_condition(CFG_toupper(condition));

  domain->set_config(cfg);
}

void Ocla::add_trigger(uint32_t domain_id, uint32_t probe_id,
                       std::string signal_name, std::string type,
                       std::string event, uint32_t value,
                       uint32_t compare_width) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;
  OclaSignal *signal = nullptr;
  OclaProbe *probe = nullptr;
  OclaInstance *instance = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain, probe_id, &probe,
                        signal_name, &signal)) {
    return;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return;
  }

  // sanity check for trigger type and event pair
  if (!CFG_type_event_sanity_check(type, event)) {
    CFG_POST_ERR("Invalid '%s' event for '%s' trigger", event.c_str(),
                 type.c_str());
    return;
  }

  if (signal->get_type() == oc_signal_type_t::PLACEHOLDER) {
    CFG_POST_ERR("Cannot setup trigger on constant signal '%s'",
                 signal->get_name().c_str());
    return;
  }

  // count the number of triggers configured this instance index
  uint32_t trigger_count = 0;
  for (auto trig : domain->get_triggers()) {
    if (probe->get_instance_index() == trig.instance_index) {
      ++trigger_count;
    }
  }

  for (auto &elem : domain->get_instances()) {
    if (elem.get_index() == probe->get_instance_index()) {
      instance = &elem;
      break;
    }
  }

  if (!instance) {
    CFG_POST_ERR("Instance %d not found", probe->get_instance_index());
    return;
  }

  // check hardware trigger slot is enuf (ocla debug info doesn't have this
  // data)
  OclaIP ocla_ip{m_adapter, instance->get_baseaddr()};
  if (trigger_count >= ocla_ip.get_trigger_count()) {
    CFG_POST_ERR("Maximum triggers reached");
    return;
  }

  // add trigger to debug session
  oc_trigger_t trig{};

  trig.instance_index = probe->get_instance_index();
  trig.probe_id = probe_id;
  trig.signal_id = signal->get_index();
  trig.signal_name = signal->get_name();
  trig.cfg.probe_num = signal->get_bitpos();
  trig.cfg.type = convert_trigger_type(type);
  trig.cfg.event = convert_trigger_event(event);
  trig.cfg.value = value;
  trig.cfg.compare_width = compare_width;

  domain->add_trigger(trig);
}

void Ocla::edit_trigger(uint32_t domain_id, uint32_t trigger_id,
                        uint32_t probe_id, std::string signal_name,
                        std::string type, std::string event, uint32_t value,
                        uint32_t compare_width) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;
  OclaSignal *signal = nullptr;
  OclaProbe *probe = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain, probe_id, &probe,
                        signal_name, &signal)) {
    return;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return;
  }

  if (trigger_id == 0 || domain->get_triggers().size() < trigger_id) {
    CFG_POST_ERR("Trigger %d not found", trigger_id);
    return;
  }

  // sanity check for trigger type and event pair
  if (!CFG_type_event_sanity_check(type, event)) {
    CFG_POST_ERR("Invalid '%s' event for '%s' trigger", event.c_str(),
                 type.c_str());
    return;
  }

  if (signal->get_type() == oc_signal_type_t::PLACEHOLDER) {
    CFG_POST_ERR("Cannot setup trigger on constant signal '%s'",
                 signal->get_name().c_str());
    return;
  }

  // modify trigger in debug session
  oc_trigger_t &trig = domain->get_triggers()[trigger_id - 1];

  trig.instance_index = probe->get_instance_index();
  trig.probe_id = probe_id;
  trig.signal_id = signal->get_index();
  trig.signal_name = signal->get_name();
  trig.cfg.probe_num = signal->get_bitpos();
  trig.cfg.type = convert_trigger_type(type);
  trig.cfg.event = convert_trigger_event(event);
  trig.cfg.value = value;
  trig.cfg.compare_width = compare_width;
}

void Ocla::remove_trigger(uint32_t domain_id, uint32_t trigger_id) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain)) {
    return;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return;
  }

  if (trigger_id == 0 || domain->get_triggers().size() < trigger_id) {
    CFG_POST_ERR("Trigger %d not found", trigger_id);
    return;
  }

  // remove trigger
  auto &triggers = domain->get_triggers();
  triggers.erase(triggers.begin() + (trigger_id - 1));
}

bool Ocla::get_hier_objects(uint32_t session_id, OclaDebugSession *&session,
                            uint32_t domain_id, OclaDomain **domain,
                            uint32_t probe_id, OclaProbe **probe,
                            std::string signal_name, OclaSignal **signal) {
  if (session_id > 0 && m_sessions.size() >= session_id) {
    session = &m_sessions[session_id - 1];
  }

  if (!session) {
    CFG_POST_ERR("Debug session is not loaded.");
    return false;
  }

  if (domain != nullptr) {
    for (auto &elem : session->get_clock_domains()) {
      if (elem.get_index() == domain_id) {
        *domain = &elem;
        break;
      }
    }

    if (!(*domain)) {
      CFG_POST_ERR("Clock domain %d not found", domain_id);
      return false;
    }
  }

  if (probe != nullptr) {
    for (auto &elem : (*domain)->get_probes()) {
      if (elem.get_index() == probe_id) {
        *probe = &elem;
        break;
      }
    }

    if (!(*probe)) {
      CFG_POST_ERR("Probe %d not found", probe_id);
      return false;
    }
  }

  if (signal != nullptr) {
    bool status = false;
    uint32_t signal_id =
        (uint32_t)CFG_convert_string_to_u64(signal_name, false, &status);

    for (auto &elem : (*probe)->get_signals()) {
      if (status) {
        if (elem.get_index() == signal_id) {
          *signal = &elem;
          break;
        }
      } else {
        if (elem.get_name() == signal_name) {
          *signal = &elem;
          break;
        }
      }
    }

    if (!(*signal)) {
      CFG_POST_ERR("Signal '%s' not found", signal_name.c_str());
      return false;
    }
  }

  return true;
}

void Ocla::show_signal_table(std::vector<OclaSignal> signals_list) {
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

  for (auto &sig : signals_list) {
    CFG_POST_MSG("  | %5d | %-35s | %-12d | %-12d |", sig.get_index(),
                 sig.get_name().c_str(), sig.get_bitpos(), sig.get_bitwidth());
  }

  CFG_POST_MSG(
      "  "
      "+-------+-------------------------------------+--------------+------"
      "--------+");
}

void Ocla::show_info() {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  if (!get_hier_objects(1, session)) {
    return;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return;
  }

  CFG_POST_MSG("User design loaded: %s", session->get_filepath().c_str());

  for (auto &domain : session->get_clock_domains()) {
    CFG_POST_MSG("Clock Domain %d:", domain.get_index());
    for (auto &probe : domain.get_probes()) {
      CFG_POST_MSG("  Probe %d", probe.get_index());
      show_signal_table(probe.get_signals());
    }

    ocla_config cfg = domain.get_config();

    CFG_POST_MSG("  OCLA Configuration");
    CFG_POST_MSG("    Mode: %s",
                 convert_ocla_trigger_mode_to_string(cfg.mode).c_str());
    CFG_POST_MSG("    Trigger Condition: %s",
                 convert_trigger_condition_to_string(cfg.condition).c_str());
    CFG_POST_MSG("    Enable Fixed Sample Size: %s",
                 cfg.enable_fix_sample_size ? "TRUE" : "FALSE");

    uint32_t sample_size = cfg.sample_size;
    if (!cfg.enable_fix_sample_size) {
      // show memory depth when fns is disabled. use first instance available
      // since all will have same memory depth
      for (auto &inst : domain.get_instances()) {
        sample_size = inst.get_memory_depth();
        break;
      }
    }

    CFG_POST_MSG("    Sample Size: %d", sample_size);

    auto triggers = domain.get_triggers();

    CFG_POST_MSG("  OCLA Trigger Configuration");
    if (triggers.size() > 0) {
      uint32_t i = 1;
      for (auto &trig : triggers) {
        auto event_name = convert_trigger_event_to_string(trig.cfg.event);
        auto type_name = convert_trigger_type_to_string(trig.cfg.type);
        switch (trig.cfg.type) {
          case EDGE:
          case LEVEL:
            CFG_POST_MSG("    #%d: signal=Probe %d/%s (#%d); mode=%s_%s", i,
                         trig.probe_id, trig.signal_name.c_str(),
                         trig.signal_id, event_name.c_str(), type_name.c_str());
            break;
          case VALUE_COMPARE:
            CFG_POST_MSG(
                "    #%d: signal=Probe %d/%s (#%d); mode=%s; "
                "compare_operator=%s; "
                "compare_value=0x%x; compare_width=%d",
                i, trig.probe_id, trig.signal_name.c_str(), trig.signal_id,
                type_name.c_str(), event_name.c_str(), trig.cfg.value,
                trig.cfg.compare_width);
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

  OclaDebugSession *session = nullptr;
  if (!get_hier_objects(1, session)) {
    return;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return;
  }

  CFG_POST_MSG("User design loaded   : %s", session->get_filepath().c_str());

  for (auto const &instance : session->get_instances()) {
    OclaIP ocla_ip{m_adapter, instance.get_baseaddr()};

    CFG_POST_MSG("OCLA %d", instance.get_index() + 1);
    CFG_POST_MSG("  Base address       : 0x%08x", instance.get_baseaddr());
    CFG_POST_MSG("  Type               : '%s'", ocla_ip.get_type().c_str());
    CFG_POST_MSG("  Version            : 0x%08x", ocla_ip.get_version());
    CFG_POST_MSG("  ID                 : 0x%08x", ocla_ip.get_id());
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

    auto probes = session->get_probes(instance.get_index());
    if (probes.size() > 0) {
      CFG_POST_MSG("  Signal Table");
      std::vector<OclaSignal> signal_list{};
      for (auto &probe : probes) {
        auto list = probe.get_signals();
        signal_list.insert(signal_list.end(), list.begin(), list.end());
      }
      show_signal_table(signal_list);
    }

    CFG_POST_MSG(" ");
  }
}

bool Ocla::get_waveform(uint32_t domain_id, oc_waveform_t &output) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain)) {
    return false;
  }

  // retrieve samples from all OCLA instances of the clock domain
  std::map<uint32_t, ocla_data> sample_data{};

  for (auto &instance : domain->get_instances()) {
    OclaIP ocla_ip{m_adapter, instance.get_baseaddr()};
    sample_data[instance.get_index()] = ocla_ip.get_data();
  }

  // transform flat sample data into logical format by probes and signals
  std::vector<oc_probe_t> probes{};

  for (auto &probe : domain->get_probes()) {
    oc_probe_t probe_data{};

    auto &data = sample_data[probe.get_instance_index()];

    for (auto &signal : probe.get_signals()) {
      oc_signal_t signal_data{};

      signal_data.name = signal.get_name();
      signal_data.bitwidth = signal.get_bitwidth();
      signal_data.bitpos = signal.get_bitpos();
      signal_data.words_per_line = ((signal.get_bitwidth() - 1) / 32) + 1;
      signal_data.depth = data.depth;
      signal_data.values.assign(signal_data.words_per_line * signal_data.depth,
                                0);

      // copy sample
      for (uint32_t i = 0; i < data.depth; i++) {
        CFG_copy_bits_vec32(
            &data.values.at(i * data.words_per_line), signal_data.bitpos,
            &signal_data.values.at(i * signal_data.words_per_line), 0,
            signal_data.bitwidth);
      }

      probe_data.signal_list.push_back(signal_data);
    }

    probe_data.probe_id = probe.get_index();
    probes.push_back(probe_data);
  }

  output.domain_id = domain->get_index();
  output.probes = probes;

  return true;
}

bool Ocla::get_status(uint32_t domain_id, uint32_t &status) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;
  OclaInstance *instance = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain)) {
    return false;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return false;
  }

  for (auto &elem : domain->get_instances()) {
    instance = &elem;
    break;
  }

  if (!instance) {
    CFG_POST_MSG("No instance found for clock domain %d", domain_id);
    return false;
  }

  // NOTE:
  // Assuming multiple instances for SINGLE clock domain will be daisy chained.
  // So only query the status of the first instance.
  OclaIP ocla_ip{m_adapter, instance->get_baseaddr()};
  status = (uint32_t)ocla_ip.get_status();

  return true;
}

void Ocla::program(OclaDomain *domain) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(domain != nullptr);

  ocla_config cfg = domain->get_config();
  ocla_trigger_config trig_cfg{};

  // default values used to clear the ocla channels
  trig_cfg.type = ocla_trigger_type::TRIGGER_NONE;
  trig_cfg.event = ocla_trigger_event::NO_EVENT;
  trig_cfg.value = 0;
  trig_cfg.compare_width = 0;
  trig_cfg.probe_num = 0;

  for (auto instance : domain->get_instances()) {
    OclaIP ocla_ip{m_adapter, instance.get_baseaddr()};
    uint32_t ch = 0;

    // program ip operation modes
    ocla_ip.configure(cfg);

    // clear all channel config on the ip
    for (uint32_t i = 0; i < ocla_ip.get_trigger_count(); i++) {
      ocla_ip.configure_channel(i, trig_cfg);
    }

    // program trigger config
    for (auto trig : domain->get_triggers()) {
      if (instance.get_index() == trig.instance_index) {
        ocla_ip.configure_channel(ch++, trig.cfg);
      }
    }
  }
}

bool Ocla::verify(OclaDebugSession *session) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(session != nullptr);

  // check probe sum (in json parser)
  uint32_t error_count = 0;

  for (auto &domain : session->get_clock_domains()) {
    for (auto &instance : domain.get_instances()) {
      OclaIP ocla_ip{m_adapter, instance.get_baseaddr()};

      if (ocla_ip.get_type() != instance.get_type()) {
        CFG_POST_ERR("Could not detect instance %d at 0x%08x",
                     instance.get_index(), instance.get_baseaddr());
        ++error_count;
        continue;
      }

      if (ocla_ip.get_version() != instance.get_version()) {
        CFG_POST_ERR(
            "Instance %d version mismatched (expected=0x%x, actual=0x%x)",
            instance.get_index(), instance.get_version(),
            ocla_ip.get_version());
        ++error_count;
      }

      if (ocla_ip.get_id() != instance.get_id()) {
        CFG_POST_ERR("Instance %d ID mismatched (expected=0x%x, actual=0x%x)",
                     instance.get_index(), instance.get_id(), ocla_ip.get_id());
        ++error_count;
      }

      if (ocla_ip.get_memory_depth() != instance.get_memory_depth()) {
        CFG_POST_ERR(
            "Instance %d memory depth mismatched (expected=%d, actual=%d)",
            instance.get_index(), instance.get_memory_depth(),
            ocla_ip.get_memory_depth());
        ++error_count;
      }

      if (ocla_ip.get_number_of_probes() != instance.get_num_of_probes()) {
        CFG_POST_ERR(
            "Instance %d no. of probes mismatched (expected=%d, actual=%d)",
            instance.get_index(), instance.get_num_of_probes(),
            ocla_ip.get_number_of_probes());
        ++error_count;
      }
    }
  }

  if (error_count > 0) {
    CFG_POST_ERR("IP Verification failed");
    return false;
  }

  return true;
}

bool Ocla::start(uint32_t domain_id) {
  CFG_ASSERT(m_adapter != nullptr);

  OclaDebugSession *session = nullptr;
  OclaDomain *domain = nullptr;
  OclaInstance *instance = nullptr;

  if (!get_hier_objects(1, session, domain_id, &domain)) {
    return false;
  }

  // verify if the ip matches the ocla debug info
  if (!verify(session)) {
    return false;
  }

  if (domain->get_triggers().empty()) {
    CFG_POST_ERR("No trigger configuration setup");
    return false;
  }

  for (auto &elem : domain->get_instances()) {
    instance = &elem;  // first instance
    break;
  }

  if (!instance) {
    CFG_POST_ERR("No instance found for clock domain %d", domain_id);
    return false;
  }

  // program the ocla ip operation modes and trigger channel configuration
  // before start
  program(domain);

  // NOTE:
  // Assuming multiple instances for SINGLE clock domain will be daisy chained.
  // So only start the first instance.
  OclaIP ocla_ip{m_adapter, instance->get_baseaddr()};
  ocla_ip.start();

  return true;
}

void Ocla::start_session(std::string filepath) {
  CFG_ASSERT(m_adapter != nullptr);

  // NOTE:
  // Currently only support 1 debug session. This can be easily extended to
  // support multiple debug sessions in the future.
  if (!m_sessions.empty()) {
    CFG_POST_ERR("Debug session is already loaded");
    return;
  }

  if (!std::filesystem::exists(filepath)) {
    CFG_POST_ERR("File '%s' not found", filepath.c_str());
    return;
  }

  std::vector<std::string> error_messages{};
  OclaDebugSession session{};

  if (session.load(filepath, error_messages)) {
    m_sessions.push_back(session);
  } else {
    // print loading/parsing error message if any returned
    for (auto &msg : error_messages) {
      CFG_POST_ERR("%s", msg.c_str());
    }
    CFG_POST_ERR("Failed load user design");
  }
}

void Ocla::stop_session() {
  CFG_ASSERT(m_adapter != nullptr);
  if (m_sessions.empty()) {
    CFG_POST_ERR("Debug session is not loaded");
    return;
  }
  m_sessions.clear();
}
