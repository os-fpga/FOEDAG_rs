#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class OclaIP;
class OclaJtagAdapter;
class OclaWaveformWriter;
class OclaSession;
struct CFGCommon_ARG;
struct Ocla_INSTANCE_INFO;
struct Ocla_PROBE_INFO;

class Ocla {
 public:
  Ocla(OclaJtagAdapter *adapter, OclaSession *session,
       OclaWaveformWriter *writer);
  void configure(uint32_t instance, std::string mode, std::string condition,
                 uint32_t sample_size);
  void configure_channel(uint32_t instance, uint32_t channel, std::string type,
                         std::string event, uint32_t value,
                         uint32_t compare_width, std::string probe);
  bool start(uint32_t instance, uint32_t timeout, std::string output_filepath);
  void start_session(std::string bitasm_filepath);
  void stop_session();
  std::string show_status(uint32_t instance);
  std::string show_info();
  std::string show_session_info();

 private:
  OclaIP get_ocla_instance(uint32_t instance);
  std::map<uint32_t, OclaIP> detect_ocla_instances();
  bool get_instance_info(uint32_t base_addr, Ocla_INSTANCE_INFO &instance_info,
                         uint32_t &idx);
  std::vector<Ocla_PROBE_INFO> get_probe_info(uint32_t base_addr);
  std::map<uint32_t, Ocla_PROBE_INFO> find_probe_info_by_name(
      uint32_t base_addr, std::string probe_name);
  bool find_probe_info_by_offset(uint32_t base_addr, uint32_t bit_offset,
                                 Ocla_PROBE_INFO &output);
  bool validate();
  OclaJtagAdapter *m_adapter;
  OclaSession *m_session;
  OclaWaveformWriter *m_writer;
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
