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
#define OCSR (0x00)
#define TBDR (0x04)
#define TCUR0 (0x08)
#define TMTR (0x0C)
#define TDCR (0x10)
#define TCUR1 (0x14)
#define IP_TYPE (0x18)
#define IP_VERSION (0x1C)
#define IP_ID (0x20)

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
  uint32_t width;
  uint32_t num_reads;
  std::vector<uint32_t> values;
};

class OclaIP {
 public:
  OclaIP();
  OclaIP(JtagAdapter *adapter, uint32_t base_addr);
  virtual ~OclaIP();
  void configure(ocla_config &cfg);
  void configureChannel(uint32_t channel, ocla_trigger_config &trig_cfg);
  void start();
  ocla_config getConfig() const;
  ocla_trigger_config getChannelConfig(uint32_t channel) const;
  ocla_status getStatus() const;
  uint32_t getNumberOfProbes() const;
  uint32_t getMemoryDepth() const;
  uint32_t getVersion() const;
  uint32_t getType() const;
  uint32_t getId() const;
  ocla_data getData() const;
  uint32_t getBaseAddr() const { return m_base_addr; }

 private:
  void configureTrigger(uint32_t addr, uint32_t offset,
                        ocla_trigger_config &trig_cfg);
  JtagAdapter *m_adapter;
  uint32_t m_base_addr;
};

#endif  //__OCLAIP_H__
