#ifndef __OCLADOMAIN_H__
#define __OCLADOMAIN_H__

#include <cstdint>
#include <string>
#include <vector>

#include "OclaIP.h"
#include "OclaInstance.h"
#include "OclaProbe.h"

enum oc_domain_type_t { NATIVE, AXI };

struct oc_trigger_t {
  ocla_trigger_config cfg;
  uint32_t instance_index;
  uint32_t probe_id;
  uint32_t signal_id;
  std::string signal_name;
  // fields to support sub bit range selection in the selected signal
  bool bitrange_enable;
  uint32_t pos;
  uint32_t width;
};

class OclaDomain {
 private:
  std::vector<OclaInstance> m_instances;
  std::vector<OclaProbe> m_probes;
  oc_domain_type_t m_type;
  uint32_t m_index;
  std::vector<oc_trigger_t> m_triggers;
  ocla_config m_config;

 public:
  OclaDomain(oc_domain_type_t type, uint32_t idx = 0);
  ~OclaDomain();
  void add_probe(const OclaProbe& probe);
  void add_instance(const OclaInstance& instance);
  oc_domain_type_t get_type() const;
  uint32_t get_index() const;
  void set_index(uint32_t idx);
  std::vector<OclaProbe>& get_probes();
  std::vector<OclaInstance>& get_instances();
  bool get_instance(uint32_t instance_index, OclaInstance*& output);
  ocla_config get_config() const;
  void set_config(ocla_config& config);
  void add_trigger(oc_trigger_t& trig);
  std::vector<oc_trigger_t>& get_triggers();
  bool get_trigger(uint32_t trigger_index, oc_trigger_t*& output);
  uint32_t get_number_of_triggers(uint32_t instance_index);
  bool remove_trigger(uint32_t trigger_index);
};

#endif  //__OCLADOMAIN_H__
