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

// OCSR (OCLA Status Register) bitfield definitions
#define OCSR_DA_Pos (0)
#define OCSR_DA_Width (1)
#define OCSR_DA_Msk (((1u << OCSR_DA_Width) - 1) << OCSR_DA_Pos)
#define OCSR_TC_Pos (24)
#define OCSR_TC_Width (5)
#define OCSR_TC_Msk (((1u << OCSR_TC_Width) - 1) << OCSR_TC_Pos)
#define OCSR_MCVS_Pos (29)
#define OCSR_MCVS_Width (3)
#define OCSR_MVCS_Msk (((1u << OCSR_MCVS_Width) - 1) << OCSR_MCVS_Pos)

// UIDP0 (User IP Defined Param 0) bitfield definitions
#define UIDP0_MD_Pos (0)
#define UIDP0_MD_Width (15)
#define UIDP0_MD_Msk (((1u << UIDP0_MD_Width) - 1) << UIDP0_MD_Pos)

// UIDP1 (User IP Defined Param 1) bitfield definitions
#define UIDP1_NP_Pos (0)
#define UIDP1_NP_Width (15)
#define UIDP1_NP_Msk (((1u << UIDP1_NP_Width) - 1) << UIDP1_NP_Pos)

// TMTR (Trigger Mode Type Register) bitfield definitions
#define TMTR_TM_Pos (0)
#define TMTR_TM_Width (2)
#define TMTR_TM_Msk (((1u << TMTR_TM_Width) - 1) << TMTR_TM_Pos)
#define TMTR_B_Pos (2)
#define TMTR_B_Width (2)
#define TMTR_B_Msk (((1u << TMTR_B_Width) - 1) << TMTR_B_Pos)
#define TMTR_FNS_Pos (4)
#define TMTR_FNS_Width (1)
#define TMTR_FNS_Msk (((1u << TMTR_FNS_Width) - 1) << TMTR_FNS_Pos)
#define TMTR_NS_Pos (12)
#define TMTR_NS_Width (20)
#define TMTR_NS_Msk (((1u << TMTR_NS_Width) - 1) << TMTR_NS_Pos)

// TSSR (Trigger Source Selection Register) bitfield definitions
#define TSSR_PS_Pos (0)
#define TSSR_PS_Width (10)
#define TSSR_PS_Msk (((1u << TSSR_PS_Width) - 1) << TSSR_PS_Pos)
#define TSSR_CW_Pos (24)
#define TSSR_CW_Width (5)
#define TSSR_CW_Msk (((1u << TSSR_CW_Width) - 1) << TSSR_CW_Pos)

// TCUR (Trigger Control Unit Register) bitfield definitions
#define TCUR_TT_Pos (0)
#define TCUR_TT_Width (2)
#define TCUR_TT_Msk (((1u << TCUR_TT_Width) - 1) << TCUR_TT_Pos)
#define TCUR_ET_Pos (2)
#define TCUR_ET_Width (2)
#define TCUR_ET_Msk (((1u << TCUR_ET_Width) - 1) << TCUR_ET_Pos)
#define TCUR_LT_Pos (4)
#define TCUR_LT_Width (2)
#define TCUR_LT_Msk (((1u << TCUR_LT_Width) - 1) << TCUR_LT_Pos)
#define TCUR_VC_Pos (6)
#define TCUR_VC_Width (2)
#define TCUR_VC_Msk (((1u << TCUR_VC_Width) - 1) << TCUR_VC_Pos)

// OCCR (OCLA Control Register) bitfield definitions
#define OCCR_ST_Pos (0)
#define OCCR_ST_Width (1)
#define OCCR_ST_Msk (((1u << OCCR_ST_Width) - 1) << OCCR_ST_Pos)
#define OCCR_SR_Pos (1)
#define OCCR_SR_Width (1)
#define OCCR_SR_Msk (((1u << OCCR_SR_Width) - 1) << OCCR_SR_Pos)

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
