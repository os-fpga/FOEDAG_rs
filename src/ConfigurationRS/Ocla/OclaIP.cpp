#include "OclaIP.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaJtagAdapter.h"

OclaIP::OclaIP(OclaJtagAdapter *adapter, uint32_t base_addr)
    : m_adapter(adapter),
      m_base_addr(base_addr),
      m_type(0),
      m_version(0),
      m_id(0),
      m_uidp0(0),
      m_uidp1(0),
      m_ocsr(0),
      m_tmtr(0),
      m_chregs({}) {
  read_registers();
}

OclaIP::OclaIP() : m_adapter(nullptr), m_base_addr(0) {}

OclaIP::~OclaIP() {}

ocla_status OclaIP::get_status() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto ocsr = m_adapter->read(m_base_addr + OCSR);
  return (ocla_status)((ocsr & OCSR_DA_Msk) >> OCSR_DA_Pos);
}

uint32_t OclaIP::get_trigger_count() const {
  uint32_t tc = (m_ocsr & OCSR_TC_Msk) >> OCSR_TC_Pos;
  return tc + 1;
}

uint32_t OclaIP::get_max_compare_value_size() const {
  uint32_t mcvs = (m_ocsr & OCSR_MVCS_Msk) >> OCSR_MCVS_Pos;
  return (mcvs + 1) * 32;
}

uint32_t OclaIP::get_number_of_probes() const {
  uint32_t np = (m_uidp1 & UIDP1_NP_Msk) >> UIDP1_NP_Pos;
  return np;
}

uint32_t OclaIP::get_memory_depth() const {
  uint32_t md = (m_uidp0 & UIDP0_MD_Msk) >> UIDP0_MD_Pos;
  return md;
}

std::string OclaIP::get_type() const {
  char buffer[10];
  uint32_t type = CFG_reverse_byte_order_u32(m_type);
  snprintf(buffer, sizeof(buffer), "%.*s", 4, (char *)&type);
  return std::string(buffer);
}

uint32_t OclaIP::get_version() const { return m_version; }

uint32_t OclaIP::get_id() const { return m_id; }

void OclaIP::configure(ocla_config &cfg) {
  CFG_ASSERT(m_adapter != nullptr);

  CFG_set_bitfield_u32(m_tmtr, TMTR_TM_Pos, TMTR_B_Width, (uint32_t)cfg.mode);
  CFG_set_bitfield_u32(m_tmtr, TMTR_B_Pos, TMTR_B_Width,
                       (uint32_t)cfg.condition);
  CFG_set_bitfield_u32(m_tmtr, TMTR_FNS_Pos, TMTR_FNS_Width,
                       (uint32_t)(cfg.sample_size > 0));

  // NOTE: When FNS is 0, NS *must* be 0 as well otherwise the OCLA will stuck
  // at sampling data forever and not setting the DA flag even internal FIFO is
  // full
  CFG_set_bitfield_u32(m_tmtr, TMTR_NS_Pos, TMTR_NS_Width, cfg.sample_size);

  m_adapter->write(m_base_addr + TMTR, m_tmtr);
}

void OclaIP::configure_channel(uint32_t channel, ocla_trigger_config &cfg) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(channel < m_chregs.size());

  ocla_channel_register reg = m_chregs[channel];

  CFG_set_bitfield_u32(reg.tcur, TCUR_TT_Pos, TCUR_TT_Width,
                       (uint32_t)cfg.type);
  CFG_set_bitfield_u32(reg.tssr, TSSR_PS_Pos, TSSR_PS_Width, cfg.probe_num);

  switch (cfg.type) {
    case EDGE:
      CFG_set_bitfield_u32(reg.tcur, TCUR_ET_Pos, TCUR_ET_Width,
                           ((uint32_t)cfg.event & 0xf));
      break;
    case LEVEL:
      CFG_set_bitfield_u32(reg.tcur, TCUR_LT_Pos, TCUR_LT_Width,
                           ((uint32_t)cfg.event & 0xf));
      break;
    case VALUE_COMPARE:
      CFG_ASSERT(cfg.compare_width > 0);
      CFG_ASSERT(cfg.compare_width <= get_max_compare_value_size());
      CFG_set_bitfield_u32(reg.tcur, TCUR_VC_Pos, TCUR_VC_Width,
                           ((uint32_t)cfg.event & 0xf));
      CFG_set_bitfield_u32(reg.tssr, TSSR_CW_Pos, TSSR_CW_Width,
                           cfg.compare_width - 1);
      reg.tdcr = cfg.value;
      break;
    case TRIGGER_NONE:
      break;
  }

  m_chregs[channel] = reg;
  m_adapter->write(m_base_addr + TSSR + (channel * 0x30), reg.tssr);
  m_adapter->write(m_base_addr + TCUR + (channel * 0x30), reg.tcur);
  m_adapter->write(m_base_addr + TDCR + (channel * 0x30), reg.tdcr);
}

void OclaIP::reset() {
  CFG_ASSERT(m_adapter != nullptr);
  m_adapter->write(m_base_addr + OCCR, (1u << OCCR_SR_Pos));
}

void OclaIP::start() {
  CFG_ASSERT(m_adapter != nullptr);

  /*
    NOTE:
    In case of back to back activation, it requires a dummy write to the OCCR
    register before setting ST bit for the start samplling to work.
  */
  m_adapter->write(m_base_addr + OCCR, 0);

  m_adapter->write(m_base_addr + OCCR, (1u << OCCR_ST_Pos));
}

ocla_data OclaIP::get_data() const {
  CFG_ASSERT(m_adapter != nullptr);

  ocla_data data;

  if (m_tmtr & TMTR_FNS_Msk) {
    data.depth = get_config().sample_size;
  } else {
    data.depth = get_memory_depth();
  }

  data.width = get_number_of_probes();
  data.words_per_line = ((data.width - 1) / 32) + 1;
  auto result =
      m_adapter->read(m_base_addr + TBDR, data.depth * data.words_per_line);
  for (auto const &value : result) {
    data.values.push_back(value.data);
  }
  return data;
}

void OclaIP::read_registers() {
  CFG_ASSERT(m_adapter != nullptr);

  // latch all read-only registers
  m_type = m_adapter->read(m_base_addr + IP_TYPE);
  m_version = m_adapter->read(m_base_addr + IP_VERSION);
  m_id = m_adapter->read(m_base_addr + IP_ID);
  m_uidp0 = m_adapter->read(m_base_addr + UIDP0);
  m_uidp1 = m_adapter->read(m_base_addr + UIDP1);
  m_ocsr = m_adapter->read(m_base_addr + OCSR);

  // latch the global configuration register
  m_tmtr = m_adapter->read(m_base_addr + TMTR);

  // latch all channle configuration registers
  for (uint32_t i = 0; i < get_trigger_count(); i++) {
    ocla_channel_register reg{};
    reg.tssr = m_adapter->read(m_base_addr + TSSR + (i * 0x30));
    reg.tcur = m_adapter->read(m_base_addr + TCUR + (i * 0x30));
    reg.tdcr = m_adapter->read(m_base_addr + TDCR + (i * 0x30));
    reg.mask = m_adapter->read(m_base_addr + MASK + (i * 0x30));
    m_chregs.push_back(reg);
  }
}

ocla_config OclaIP::get_config() const {
  ocla_config cfg;

  cfg.mode = (ocla_trigger_mode)((m_tmtr & TMTR_TM_Msk) >> TMTR_TM_Pos);
  cfg.condition = (ocla_trigger_condition)((m_tmtr & TMTR_B_Msk) >> TMTR_B_Pos);
  cfg.sample_size = ((m_tmtr & TMTR_NS_Msk) >> TMTR_NS_Pos);

  return cfg;
}

ocla_trigger_config OclaIP::get_channel_config(uint32_t channel) const {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(channel < m_chregs.size());

  ocla_channel_register reg = m_chregs[channel];
  ocla_trigger_config cfg;
  cfg.probe_num = (reg.tssr & TSSR_PS_Msk) >> TSSR_PS_Pos;
  cfg.type = (ocla_trigger_type)((reg.tcur & TCUR_TT_Msk) >> TCUR_TT_Pos);
  cfg.compare_width = 0;
  cfg.value = 0;

  switch (cfg.type) {
    case EDGE:
      cfg.event =
          (ocla_trigger_event)(((reg.tcur & TCUR_ET_Msk) >> TCUR_ET_Pos) |
                               0x10);
      break;
    case LEVEL:
      cfg.event =
          (ocla_trigger_event)(((reg.tcur & TCUR_LT_Msk) >> TCUR_LT_Pos) |
                               0x20);
      break;
    case VALUE_COMPARE:
      cfg.event =
          (ocla_trigger_event)(((reg.tcur & TCUR_VC_Msk) >> TCUR_VC_Pos) |
                               0x30);
      cfg.value = reg.tdcr;
      cfg.compare_width = ((reg.tssr & TSSR_CW_Msk) >> TSSR_CW_Pos) + 1;
      break;
    case TRIGGER_NONE:
      cfg.event = ocla_trigger_event::NO_EVENT;
      break;
  }

  return cfg;
}
