#ifndef __OCLASESSION_H__
#define __OCLASESSION_H__

#include <cstdint>
#include <string>
#include <vector>

struct Ocla_INSTANCE_INFO {
  uint32_t version;
  std::string type;
  uint32_t id;
  uint32_t base_addr;
  uint32_t depth;
  uint32_t num_probes;
  uint32_t num_trigger_inputs;
  uint32_t probe_width;
  uint32_t axi_addr_width;
  uint32_t axi_data_width;
};

enum signal_type { SIGNAL, PLACEHOLDER };

struct Ocla_PROBE_INFO {
  std::string signal_name;
  uint32_t bitwidth;
  uint32_t value;
  signal_type type;
};

class OclaSession {
 public:
  virtual ~OclaSession(){};
  virtual bool is_loaded() const = 0;
  virtual void load(std::string bitasm_filepath) = 0;
  virtual void unload() = 0;
  virtual uint32_t get_instance_count() = 0;
  virtual Ocla_INSTANCE_INFO get_instance_info(uint32_t instance) = 0;
  virtual std::vector<Ocla_PROBE_INFO> get_probe_info(uint32_t instance) = 0;
  virtual std::string get_bitasm_filepath() = 0;
};

#endif  //__OCLASESSION_H__
