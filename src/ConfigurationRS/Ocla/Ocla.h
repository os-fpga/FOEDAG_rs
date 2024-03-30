#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <string>
#include <vector>

#include "OclaDebugSession.h"

class OclaJtagAdapter;
class OclaWaveformWriter;
struct CFGCommon_ARG;

class Ocla {
 public:
  Ocla(OclaJtagAdapter *adapter, OclaWaveformWriter *writer);
  void configure(uint32_t domain, std::string mode, std::string condition,
                 uint32_t sample_size);
  void add_trigger(uint32_t domain, uint32_t probe, std::string signal,
                   std::string type, std::string event, uint32_t value,
                   uint32_t compare_width);
  uint32_t show_status(uint32_t domain);

#if 0
  bool start(uint32_t instance, uint32_t timeout, std::string output_filepath);
#endif

  void start_session(std::string filepath);
  void stop_session();
  void show_info();
  void show_instance_info();

 private:
  static OclaDebugSession m_session;
  OclaJtagAdapter *m_adapter;
  OclaWaveformWriter *m_writer;
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
