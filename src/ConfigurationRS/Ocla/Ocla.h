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
  bool get_domain(uint32_t domain_id, OclaDomain *&domain);
  bool get_domain_probe_signal(uint32_t domain_id, uint32_t probe_id,
                               std::string signal_name, OclaDomain *&domain,
                               OclaProbe *&probe, OclaSignal *&signal);
  bool get_session(uint32_t session_id, OclaDebugSession *&session);
  bool get_instance(uint32_t domain_id, OclaInstance *&instance);
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
