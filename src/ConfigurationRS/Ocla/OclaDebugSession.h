#ifndef __OCLADEBUGSESSION_H__
#define __OCLADEBUGSESSION_H__

#include <cstdint>
#include <string>
#include <vector>

#include "OclaDomain.h"
#include "OclaIP.h"
#include "OclaInstance.h"
#include "OclaProbe.h"
#include "OclaSignal.h"

class OclaDebugSession {
 private:
  std::vector<OclaDomain> m_clock_domains;
  std::string m_filepath;
  bool m_loaded;
  bool parse_signal(std::string signal_str, OclaSignal& signal,
                    std::vector<std::string>& error_messages);
  bool parse(std::string ocla_str, std::vector<std::string>& error_messages);

 public:
  OclaDebugSession();
  ~OclaDebugSession();
  std::vector<OclaDomain>& get_clock_domains();
  std::vector<OclaInstance> get_instances();
  std::vector<OclaProbe> get_probes(uint32_t instance_index);
  std::string get_filepath() const;
  bool is_loaded() const;
  bool load(std::string filepath, std::vector<std::string>& error_messages);
  void unload();
};

#endif  //__OCLADEBUGSESSION_H__
