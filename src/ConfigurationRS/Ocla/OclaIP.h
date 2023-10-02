#ifndef __OCLAIP_H__
#define __OCLAIP_H__

#include <cstdint>
#include <vector>

#define OCLA1_ADDR (0x02000000U)
#define OCLA2_ADDR (0x03000000U)
#define OCLA_TYPE (0x6f636c61U)
#define OCLA_TRIGGER_CHANNELS (4U)
#define OCLA_MAX_SAMPLE_SIZE (1024U)
#define OCLA_MAX_PROBE (128U)

class JtagAdapter;

enum ocla_status { NA = 0, DATA_AVAILABLE = 1 };

enum ocla_mode { NO_TRIGGER = 0, PRE = 1, POST = 2, CENTER = 3 };

enum ocla_fns { DISABLED = 0, ENABLED = 1 };

enum ocla_trigger_condition { DEFAULT = 0, AND = 1, OR = 2, XOR = 3 };

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
  ocla_mode mode;
  ocla_trigger_condition cond;
  ocla_fns fns;
  uint32_t ns;
  uint32_t st;
};

struct ocla_trigger_config {
  ocla_trigger_type type;
  ocla_trigger_event event;
  uint32_t value;
  uint32_t probe_num;
};

struct ocla_data {
  uint32_t depth;
  uint32_t linewidth;
  uint32_t reads_per_line;
  std::vector<uint32_t> values;
};

class OclaIP {
 public:
  OclaIP(JtagAdapter *adapter, uint32_t base_addr);
  virtual ~OclaIP();
  void configure(ocla_config &cfg);
  ocla_config getConfig();
  void configureChannel(uint32_t channel, ocla_trigger_config &trig_cfg);
  ocla_trigger_config getChannelConfig(uint32_t channel);
  ocla_status getStatus();
  uint32_t getNumberOfProbes();
  uint32_t getMemoryDepth();
  uint32_t getVersion();
  uint32_t getType();
  uint32_t getId();
  void start();
  ocla_data getData();
  uint32_t getBaseAddr() { return m_base_addr; }

 private:
  void configureTrigger(uint32_t addr, uint32_t offset,
                        ocla_trigger_config &trig_cfg);
  JtagAdapter *m_adapter;
  uint32_t m_base_addr;
};

#endif  //__OCLAIP_H__