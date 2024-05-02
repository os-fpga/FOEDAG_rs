#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <string>
#include <vector>

#include "OclaDebugSession.h"

class OclaJtagAdapter;
class OclaWaveformWriter;
struct CFGCommon_ARG;

struct oc_signal_t {
  std::string name;
  std::vector<uint32_t> values;
  uint32_t bitwidth;
  uint32_t bitpos;
  uint32_t words_per_line;
  uint32_t depth;
};

struct oc_probe_t {
  std::vector<oc_signal_t> signal_list;
  uint32_t probe_id;
};

struct oc_waveform_t {
  std::vector<oc_probe_t> probes;
  uint32_t domain_id;
};

struct eio_value_t {
  std::string signal_name;
  uint32_t idx;
  std::vector<uint32_t> value;
};

class Ocla {
 public:
  Ocla(OclaJtagAdapter *adapter);
  ~Ocla();
  void configure(uint32_t domain_id, std::string mode, std::string condition,
                 uint32_t sample_size);
  void add_trigger(uint32_t domain_id, uint32_t probe_id,
                   std::string signal_name, std::string type, std::string event,
                   uint32_t value, uint32_t compare_width);
  void edit_trigger(uint32_t domain_id, uint32_t trigger_index,
                    uint32_t probe_id, std::string signal_name,
                    std::string type, std::string event, uint32_t value,
                    uint32_t compare_width);
  void remove_trigger(uint32_t domain_id, uint32_t trigger_index);
  bool get_waveform(uint32_t domain_id, oc_waveform_t &output);
  bool get_status(uint32_t domain_id, uint32_t &status);
  bool start(uint32_t domain_id);
  void start_session(std::string filepath);
  bool set_io(std::vector<std::string> signal_list);
  bool get_io(std::vector<std::string> signal_list,
              std::vector<eio_value_t> &output);
  void stop_session();
  void show_info();
  void show_instance_info();

 private:
  static std::vector<OclaDebugSession> m_sessions;
  OclaJtagAdapter *m_adapter;

  bool get_session(uint32_t session_id, OclaDebugSession *&session);
  bool get_hier_objects(uint32_t session_id, OclaDebugSession *&session,
                        uint32_t domain_id = 0, OclaDomain **domain = nullptr,
                        uint32_t probe_id = 0, OclaProbe **probe = nullptr,
                        std::string signal_name = "",
                        OclaSignal **signal = nullptr);
  bool get_eio_hier_objects(uint32_t session_id, OclaDebugSession *&session,
                            uint32_t instance_index = 0,
                            EioInstance **instance = nullptr,
                            uint32_t probe_id = 0,
                            eio_probe_type_t probe_type = IO_INPUT,
                            eio_probe_t **probe = nullptr);
  void show_signal_table(std::vector<OclaSignal> &signal_list);
  void show_eio_signal_table(std::vector<eio_signal_t> &signal_list);
  void program(OclaDomain *domain);
  bool verify(OclaDebugSession *session);
  bool find_eio_signals(std::vector<eio_signal_t> &signal_list,
                        std::vector<std::string> signal_names,
                        std::vector<eio_signal_t> &output_list);
  std::string format_signal_name(oc_trigger_t &trig);
  bool parse_eio_signal_list(std::vector<std::string> signal_list,
                             std::vector<std::string> &names,
                             std::vector<std::vector<uint32_t>> &values);
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
