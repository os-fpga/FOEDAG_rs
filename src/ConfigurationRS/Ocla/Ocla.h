#ifndef __OCLA_H__
#define __OCLA_H__

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <map>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "JtagAdapter.h"
#include "OclaIP.h"

using namespace std;
using namespace std::filesystem;

void Ocla_entry(CFGCommon_ARG *cmdarg);

class Ocla {
 public:
  Ocla() : m_adapter(nullptr) {}
  void setJtagAdapter(JtagAdapter *adapter) { m_adapter = adapter; }
  void configure(uint32_t instance, string mode, string cond,
                 string sample_size);
  void configureChannel(uint32_t instance, uint32_t channel, string type,
                        string event, uint32_t value, string probe);
  void start(uint32_t instance, uint32_t timeout, string outputfilepath);
  void startSession(string bitasmFilepath);
  void stopSession();
  void showStatus(uint32_t instance, stringstream &ss);
  void showInfo(stringstream &ss);

 private:
  OclaIP getOclaInstance(uint32_t instance);
  map<uint32_t, OclaIP> detect();
  JtagAdapter *m_adapter;
};

#endif  //__OCLA_H__
