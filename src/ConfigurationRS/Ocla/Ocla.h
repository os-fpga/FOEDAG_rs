#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "OclaDebugSession.h"

class OclaJtagAdapter;
class OclaWaveformWriter;
struct CFGCommon_ARG;

class Ocla {
 public:
  Ocla(OclaJtagAdapter *adapter, OclaWaveformWriter *writer);
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
  bool get_waveform(uint32_t domain_id,
                    std::map<uint32_t, std::vector<uint32_t>> &output);
  bool get_status(uint32_t domain_id, uint32_t &status);
  bool start(uint32_t domain_id);
  void start_session(std::string filepath);
  void stop_session();
  void show_info();
  void show_instance_info();

 private:
  static std::vector<OclaDebugSession> m_sessions;
  OclaJtagAdapter *m_adapter;
  OclaWaveformWriter *m_writer;

  void program_ip(OclaIP &ocla_ip, ocla_config &config,
                  std::vector<ocla_trigger_config> &triggers);
  bool get_hier_objects(uint32_t session_id, OclaDebugSession *&session,
                        uint32_t domain_id = 0, OclaDomain **domain = nullptr,
                        uint32_t probe_id = 0, OclaProbe **probe = nullptr,
                        std::string signal_name = "",
                        OclaSignal **signal = nullptr);
  void show_signal_table(std::vector<OclaSignal> signals_list);
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
