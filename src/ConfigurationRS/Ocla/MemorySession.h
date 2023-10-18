#ifndef __MEMORYSESSION_H__
#define __MEMORYSESSION_H__

#include <map>
#include <vector>

#include "OclaSession.h"

class MemorySession : public OclaSession {
 public:
  MemorySession();
  virtual ~MemorySession();
  virtual bool is_loaded() const { return m_loaded; };
  virtual void load(std::string bitasm_filepath);
  virtual void unload();
  virtual uint32_t get_instance_count();
  virtual Ocla_INSTANCE_INFO get_instance_info(uint32_t instance);
  virtual std::vector<Ocla_PROBE_INFO> get_probe_info(uint32_t instance);
  virtual std::string get_bitasm_filepath();

 private:
  void parse(std::string ocla_json);
  Ocla_PROBE_INFO parse_probe(std::string probe);
  static std::map<uint32_t, Ocla_INSTANCE_INFO> m_instances;
  static std::map<uint32_t, std::vector<Ocla_PROBE_INFO>> m_probes;
  static bool m_loaded;
  static std::string m_bitasm_filepath;
};

#endif  //__MEMORYSESSION_H__
