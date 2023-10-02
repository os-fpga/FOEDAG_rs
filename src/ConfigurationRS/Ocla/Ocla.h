#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <map>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "JtagAdapter.h"
#include "OclaIP.h"

void Ocla_entry(CFGCommon_ARG *cmdarg);

class Ocla {
 public:
  Ocla() : m_adapter(nullptr) {}
  void setJtagAdapter(JtagAdapter *adapter) { m_adapter = adapter; }
  void configure(uint32_t instance, std::string mode, std::string cond,
                 std::string sample_size);
  void configureChannel(uint32_t instance, uint32_t channel, std::string type,
                        std::string event, uint32_t value, std::string probe);
  void start(uint32_t instance, uint32_t timeout, std::string outputfilepath);
  void startSession(std::string bitasmFilepath);
  void stopSession();
  void showStatus(uint32_t instance, std::stringstream &ss);
  void showInfo(std::stringstream &ss);

 private:
  OclaIP getOclaInstance(uint32_t instance);
  std::map<uint32_t, OclaIP> detect();
  JtagAdapter *m_adapter;
};

#endif  //__OCLA_H__
