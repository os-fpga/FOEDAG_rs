#ifndef __OCLAIP_H__
#define __OCLAIP_H__

#include <cstdint>
#include <string>
#include <vector>

#define IP_TYPE (0x00)     // [RO] This register hold the IP Type "ocla"
#define IP_VERSION (0x04)  // [RO] This register hold the IP Version
#define IP_ID (0x08)       // [RO] This register hold the IP ID
#define UIDP0 (0x14)       // [RO] Hold buffer size information
#define UIDP1 (0x18)       // [RO] Hold number of Probes
#define OCSR \
  (0x20)  // [RO] Bitfields of OCSR contains configuration status of OCLA
#define TMTR (0x24)  // [RW] TMCR is used configure trigger Modes
#define TBDR \
  (0x28)  // [RO] TBDR can be read to stream the whole acquisition data to some
          // output interface
#define OCCR \
  (0x2C)  // [RW] This register will be used to start sampling or reset the IP
          // core.
#define TSSR \
  (0x30)  // [RW] Channel 1. These registers will be used to select a probe
          // across 1024 for trigger
#define TCUR \
  (0x34)  // [RW] Channel 1. These registers will be used to set triggers, that
          // can be edge, level or value compared.  
#define TDCR \
  (0x38)  // [RW] Channel 1. Data that the user wants to compare with probe data
          // will be written to this register.  
#define MASK (0x3C)  // [RW] Channel 1. For disjoint probe comparision

class OclaJtagAdapter;

enum ocla_status { NA = 0, DATA_AVAILABLE = 1 };

enum ocla_trigger_mode { CONTINUOUS = 0, PRE = 1, POST = 2, CENTER = 3 };

enum ocla_trigger_bool_comp { DEFAULT = 0, AND = 1, OR = 2, XOR = 3 };

enum ocla_trigger_type {
  TRIGGER_NONE = 0,
  EDGE = 1,
  LEVEL = 2,
  VALUE_COMPARE = 3
};

enum ocla_trigger_event {
  NONE = 0,
  EDGE_NONE = 0x10,
  RISING = 0x11,
  FALLING = 0x12,
  EITHER = 0x13,
  LOW = 0x20,
  HIGH = 0x21,
  VALUE_NONE = 0x30,
  EQUAL = 0x31,
  LESSER = 0x32,
  GREATER = 0x33
};

struct ocla_config {
  ocla_trigger_mode mode;
  ocla_trigger_bool_comp boolcomp;
  bool fns;
  uint32_t ns;
};

struct ocla_trigger_config {
  ocla_trigger_type type;
  ocla_trigger_event event;
  uint32_t value;
  uint32_t value_bitwidth;
  uint32_t probe_num;
};

struct ocla_data {
  uint32_t depth;
  uint32_t width;
  uint32_t num_reads;
  std::vector<uint32_t> values;
};

struct ocla_channel_register {
  uint32_t tssr;
  uint32_t tcur;
  uint32_t tdcr;
  uint32_t mask;
};

class OclaIP {
 public:
  OclaIP();
  OclaIP(OclaJtagAdapter *adapter, uint32_t base_addr);
  virtual ~OclaIP();
  void configure(ocla_config &cfg);
  void configure_channel(uint32_t channel, ocla_trigger_config &trig_cfg);
  void start();
  void reset();
  ocla_config get_config() const;
  ocla_trigger_config get_channel_config(uint32_t channel) const;
  ocla_status get_status() const;
  uint32_t get_number_of_probes() const;
  uint32_t get_memory_depth() const;
  uint32_t get_trigger_count() const;
  uint32_t get_max_compare_value_size() const;
  uint32_t get_version() const;
  std::string get_type() const;
  uint32_t get_id() const;
  ocla_data get_data() const;
  uint32_t get_base_addr() const { return m_base_addr; }

 private:
  void read_registers();
  OclaJtagAdapter *m_adapter;
  uint32_t m_base_addr;
  uint32_t m_type;
  uint32_t m_version;
  uint32_t m_id;
  uint32_t m_uidp0;
  uint32_t m_uidp1;
  uint32_t m_ocsr;
  uint32_t m_tmtr;
  std::vector<ocla_channel_register> m_chregs;
};

#endif  //__OCLAIP_H__
