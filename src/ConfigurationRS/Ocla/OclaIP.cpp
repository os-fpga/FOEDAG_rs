#include "OclaIP.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaJtagAdapter.h"

uint32_t CFG_reverse_byte_order_u32(uint32_t value) {
  return (value >> 24) | ((value >> 8) & 0xff00) | ((value << 8) & 0xff0000) |
         (value << 24);
}

void CFG_set_bitfield_u32(uint32_t *value, uint8_t pos, uint8_t width,
                          uint32_t data) {
  uint32_t mask = (~0u >> (32 - width)) << pos;
  *value &= ~mask;
  *value |= (data & ((1u << width) - 1)) << pos;
}

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
  return (ocsr & 1) ? DATA_AVAILABLE : NA;
}

uint32_t OclaIP::get_trigger_count() const {
  uint32_t tc = (m_ocsr >> 24) & 0x1f;
  return tc + 1;
}

uint32_t OclaIP::get_max_compare_value_size() const {
  uint32_t mcvs = (m_ocsr >> 29) & 0x7;
  return (mcvs + 1) * 32;
}

uint32_t OclaIP::get_number_of_probes() const {
  uint32_t np = m_uidp1 & 0xffff;
  return np;
}

uint32_t OclaIP::get_memory_depth() const {
  uint32_t md = m_uidp0 & 0xffff;
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
  CFG_set_bitfield_u32(&m_tmtr, 0, 2, (uint32_t)cfg.mode);
  CFG_set_bitfield_u32(&m_tmtr, 2, 2, (uint32_t)cfg.boolcomp);
  CFG_set_bitfield_u32(&m_tmtr, 4, 1, cfg.fns ? 1 : 0);
  CFG_set_bitfield_u32(&m_tmtr, 12, 20, (cfg.ns - 1));
  m_adapter->write(m_base_addr + TMTR, m_tmtr);
}

void OclaIP::configure_channel(uint32_t channel, ocla_trigger_config &cfg) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(channel < m_chregs.size());

  ocla_channel_register reg = m_chregs[channel];

  CFG_set_bitfield_u32(&reg.tcur, 0, 2, (uint32_t)cfg.type);
  CFG_set_bitfield_u32(&reg.tssr, 0, 10, cfg.probe_num);

  switch (cfg.type) {
    case EDGE:
      CFG_set_bitfield_u32(&reg.tcur, 2, 2, ((uint32_t)cfg.event & 0xf));
      break;
    case LEVEL:
      CFG_set_bitfield_u32(&reg.tcur, 4, 2, ((uint32_t)cfg.event & 0xf));
      break;
    case VALUE_COMPARE:
      CFG_ASSERT(cfg.value_bitwidth > 0);
      CFG_ASSERT(cfg.value_bitwidth <= get_max_compare_value_size());
      CFG_set_bitfield_u32(&reg.tcur, 6, 2, ((uint32_t)cfg.event & 0xf));
      CFG_set_bitfield_u32(&reg.tssr, 24, 5, cfg.value_bitwidth - 1);
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
  m_adapter->write(m_base_addr + OCCR, (1u << 1));
}

void OclaIP::start() {
  CFG_ASSERT(m_adapter != nullptr);
  m_adapter->write(m_base_addr + OCCR, (1u << 0));
}

ocla_data OclaIP::get_data() const {
  CFG_ASSERT(m_adapter != nullptr);

  ocla_data data;

  if (m_tmtr & (1u << 4)) {
    data.depth = get_config().ns;
  } else {
    data.depth = get_memory_depth();
  }

  data.width = get_number_of_probes();
  data.num_reads = ((data.width - 1) / 32) + 1;
  data.values =
      m_adapter->read(m_base_addr + TBDR, data.depth * data.num_reads);

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

  cfg.mode = (ocla_trigger_mode)(m_tmtr & 0x3);
  cfg.boolcomp = (ocla_trigger_bool_comp)((m_tmtr >> 2) & 0x3);
  cfg.fns = (m_tmtr & (1u << 4)) ? true : false;
  cfg.ns = (m_tmtr >> 12) + 1;

  return cfg;
}

ocla_trigger_config OclaIP::get_channel_config(uint32_t channel) const {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(channel < m_chregs.size());

  ocla_channel_register reg = m_chregs[channel];
  ocla_trigger_config cfg;
  cfg.probe_num = reg.tssr & 0x3ff;
  cfg.type = (ocla_trigger_type)(reg.tcur & 0x3);
  cfg.value_bitwidth = 0;
  cfg.value = 0;

  switch (cfg.type) {
    case EDGE:
      cfg.event = (ocla_trigger_event)(((reg.tcur >> 2) & 0x3) | 0x10);
      break;
    case LEVEL:
      cfg.event = (ocla_trigger_event)(((reg.tcur >> 4) & 0x1) | 0x20);
      break;
    case VALUE_COMPARE:
      cfg.event = (ocla_trigger_event)(((reg.tcur >> 6) & 0x3) | 0x30);
      cfg.value = reg.tdcr;
      cfg.value_bitwidth = ((reg.tssr >> 24) & 0x1f) + 1;
      break;
    case TRIGGER_NONE:
      cfg.event = ocla_trigger_event::NONE;
      break;
  }

  return cfg;
}
