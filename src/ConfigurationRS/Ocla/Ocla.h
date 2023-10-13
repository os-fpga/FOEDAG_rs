#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <map>
#include <string>

class OclaIP;
class OclaJtagAdapter;
class OclaWaveformWriter;
class OclaSession;
struct CFGCommon_ARG;

class Ocla {
 public:
  Ocla(OclaJtagAdapter *adapter, OclaSession *session = nullptr,
       OclaWaveformWriter *writer = nullptr);
  void configure(uint32_t instance, std::string mode, std::string condition,
                 uint32_t sample_size);
  void configureChannel(uint32_t instance, uint32_t channel, std::string type,
                        std::string event, uint32_t value, std::string probe);
  void start(uint32_t instance, uint32_t timeout, std::string outputfilepath);
  void startSession(std::string bitasmFilepath);
  void stopSession();
  std::string showStatus(uint32_t instance);
  std::string showInfo();
  // debug use
  std::string dumpRegisters(uint32_t instance);
  std::string dumpSamples(uint32_t instance, bool dumpText,
                          bool genrateWaveform);
  void debugStart(uint32_t instance);

 private:
  OclaIP getOclaInstance(uint32_t instance);
  std::map<uint32_t, OclaIP> detectOclaInstances();
  OclaJtagAdapter *m_adapter;
  OclaSession *m_session;
  OclaWaveformWriter *m_writer;
};

void Ocla_entry(CFGCommon_ARG *cmdarg);

#endif  //__OCLA_H__
