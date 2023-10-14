#ifndef __OCLASESSION_H__
#define __OCLASESSION_H__

#include <cstdint>
#include <string>
#include <vector>

struct Ocla_INSTANCE_INFO {
  uint32_t version;
  uint32_t type;
  uint32_t id;
  uint32_t base_addr;
  uint32_t depth;
  uint32_t num_probes;
};

struct Ocla_PROBE_INFO {
  std::string signal_name;
  uint32_t bitwidth;
};

class OclaSession {
 public:
  virtual ~OclaSession(){};
  virtual bool is_loaded() const = 0;
  virtual void load(std::string bitasmfile) = 0;
  virtual void unload() = 0;
  virtual uint32_t get_instance_count() = 0;
  virtual Ocla_INSTANCE_INFO get_instance_info(uint32_t instance) = 0;
  virtual std::vector<Ocla_PROBE_INFO> get_probe_info(uint32_t instance) = 0;
};

#endif  //__OCLASESSION_H__
