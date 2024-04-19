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

class Ocla {
 public:
  Ocla(OclaJtagAdapter *adapter);
  ~Ocla();
  void configure(uint32_t domain_id, std::string mode, std::string condition,
                 uint32_t sample_size);
  void add_trigger(uint32_t domain_id, uint32_t probe_id,
                   std::string signal_name, std::string type, std::string event,
                   uint32_t value, uint32_t compare_width);
  void edit_trigger(uint32_t domain_id, uint32_t trigger_id, uint32_t probe_id,
                    std::string signal_name, std::string type,
                    std::string event, uint32_t value, uint32_t compare_width);
  void remove_trigger(uint32_t domain_id, uint32_t trigger_id);
  bool get_waveform(uint32_t domain_id, oc_waveform_t &output);
  bool get_status(uint32_t domain_id, uint32_t &status);
  bool start(uint32_t domain_id);
  void start_session(std::string filepath);
  void stop_session();
  void show_info();
  void show_instance_info();

 private:
  static std::vector<OclaDebugSession> m_sessions;
  OclaJtagAdapter *m_adapter;

  bool get_hier_objects(uint32_t session_id, OclaDebugSession *&session,
                        uint32_t domain_id = 0, OclaDomain **domain = nullptr,
                        uint32_t probe_id = 0, OclaProbe **probe = nullptr,
                        std::string signal_name = "",
                        OclaSignal **signal = nullptr);
  void show_signal_table(std::vector<OclaSignal> signals_list);
  void program(OclaDomain *domain);
  bool verify(OclaDebugSession *session);
  std::string format_signal_name(oc_trigger_t &trig);
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
