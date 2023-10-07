#include "OclaIP.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaJtagAdapter.h"

#define MAX_SAMPLE (1024)

OclaIP::OclaIP(OclaJtagAdapter *adapter, uint32_t base_addr)
    : m_adapter(adapter), m_base_addr(base_addr) {}

OclaIP::OclaIP() : m_adapter(nullptr), m_base_addr(0) {}

OclaIP::~OclaIP() {}

ocla_status OclaIP::getStatus() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto ocsr = m_adapter->read(m_base_addr + OCSR);
  return (ocsr & 1) ? DATA_AVAILABLE : NA;
}

uint32_t OclaIP::getNumberOfProbes() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto numprobes = (m_adapter->read(m_base_addr + OCSR) >> 1) & 0x3ff;
  return numprobes;
}

uint32_t OclaIP::getMemoryDepth() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto memdepth = (m_adapter->read(m_base_addr + OCSR) >> 11) & 0x3ff;
  return memdepth;
}

uint32_t OclaIP::getId() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto id = m_adapter->read(m_base_addr + IP_ID);
  return id;
}

uint32_t OclaIP::getType() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto type = m_adapter->read(m_base_addr + IP_TYPE);
  return type;
}

uint32_t OclaIP::getVersion() const {
  CFG_ASSERT(m_adapter != nullptr);
  auto version = m_adapter->read(m_base_addr + IP_VERSION);
  return version;
}

void OclaIP::configure(ocla_config &cfg) {
  CFG_ASSERT(m_adapter != nullptr);

  uint32_t tmtr = (cfg.mode << 0);
  tmtr |= (cfg.fns << 3);
  tmtr |= ((cfg.ns & 0x7ff) << 4);
  m_adapter->write(m_base_addr + TMTR, tmtr);

  for (auto &reg : {TCUR0, TCUR1}) {
    uint32_t tcur = m_adapter->read(m_base_addr + reg);
    tcur &= ~(3 << 15);
    tcur |= (cfg.cond << 15);
    m_adapter->write(m_base_addr + reg, tcur);
  }
}

void OclaIP::configureChannel(uint32_t channel, ocla_trigger_config &trig_cfg) {
  CFG_ASSERT(m_adapter != nullptr);
  configureTrigger(channel < 2 ? m_base_addr + TCUR0 : m_base_addr + TCUR1,
                   channel % 2 ? 7 : 0, trig_cfg);
}

void OclaIP::configureTrigger(uint32_t addr, uint32_t offset,
                              ocla_trigger_config &trig_cfg) {
  uint32_t tcur = m_adapter->read(addr);

  tcur &= ~(0x3 << (1 + offset));
  tcur |= (trig_cfg.type << (1 + offset));
  tcur &= ~(0x7f << (17 + offset));
  tcur |= ((trig_cfg.probe_num & 0x7f) << (17 + offset));

  switch (trig_cfg.type) {
    case EDGE:
      tcur &= ~(0x3 << (3 + offset));
      tcur |= ((trig_cfg.event & 0xf) << (3 + offset));
      break;
    case LEVEL:
      tcur &= ~(0x1 << (5 + offset));
      tcur |= ((trig_cfg.event & 0xf) << (5 + offset));
      break;
    case VALUE_COMPARE:
      tcur &= ~(0x3 << (6 + offset));
      tcur |= ((trig_cfg.event & 0xf) << (6 + offset));
      m_adapter->write(m_base_addr + TDCR, trig_cfg.value);
      break;
  }

  m_adapter->write(addr, tcur);
}

void OclaIP::start() {
  CFG_ASSERT(m_adapter != nullptr);
  /*
    NOTE:
    Do a dummy read and write on TCUR register to clear the
    internal trace buffer before starting the OCLA.
    OCLA will not start if the buffer is not cleared
    in the first place.
   */
  m_adapter->write(m_base_addr + TCUR0, m_adapter->read(m_base_addr + TCUR0));

  uint32_t tmtr = m_adapter->read(m_base_addr + TMTR);
  tmtr |= (1 << 2);
  m_adapter->write(m_base_addr + TMTR, tmtr);
}

ocla_data OclaIP::getData() const {
  CFG_ASSERT(m_adapter != nullptr);
  uint32_t ocsr = m_adapter->read(m_base_addr + OCSR);
  uint32_t tmtr = m_adapter->read(m_base_addr + TMTR);

  ocla_data data;

  if ((tmtr >> 3) & 0x1) {
    data.depth = (tmtr >> 4) & 0x7ff;
  } else {
    data.depth = (ocsr >> 11) & 0x3ff;
  }

  data.width = (ocsr >> 1) & 0x3ff;
  data.num_reads = ((data.width - 1) / 32) + 1;
  data.values =
      m_adapter->read(m_base_addr + TBDR, data.depth * data.num_reads);

  return data;
}

ocla_config OclaIP::getConfig() const {
  CFG_ASSERT(m_adapter != nullptr);
  uint32_t tmtr = m_adapter->read(m_base_addr + TMTR);
  ocla_config cfg;
  cfg.mode = (ocla_mode)(tmtr & 0x3);
  cfg.fns = (ocla_fns)((tmtr >> 3) & 0x1);
  cfg.ns = ((tmtr >> 4) & 0x7ff);
  cfg.st = ((tmtr >> 2) & 1);

  uint32_t tcur0 = m_adapter->read(m_base_addr + TCUR0);
  tcur0 = ((tcur0 >> 15) & 3);
  uint32_t tcur1 = m_adapter->read(m_base_addr + TCUR1);
  tcur1 = ((tcur1 >> 15) & 3);
  cfg.cond = tcur0 == tcur1 ? (ocla_trigger_condition)tcur0 : DEFAULT;
  return cfg;
}

ocla_trigger_config OclaIP::getChannelConfig(uint32_t channel) const {
  CFG_ASSERT(m_adapter != nullptr);
  uint32_t tcur = m_adapter->read(m_base_addr + (channel < 2 ? TCUR0 : TCUR1));
  uint32_t offset = channel % 2 ? 7 : 0;

  ocla_trigger_config trig_cfg;
  trig_cfg.probe_num = (tcur >> (17 + offset)) & 0x7f;
  trig_cfg.type = (ocla_trigger_type)((tcur >> (1 + (offset))) & 0x3);

  switch (trig_cfg.type) {
    case EDGE:
      trig_cfg.event =
          (ocla_trigger_event)(((tcur >> (3 + (offset))) & 0x3) | 0x10);
      break;
    case LEVEL:
      trig_cfg.event =
          (ocla_trigger_event)(((tcur >> (5 + (offset))) & 0x1) | 0x20);
      break;
    case VALUE_COMPARE:
      trig_cfg.event =
          (ocla_trigger_event)(((tcur >> (6 + (offset))) & 0x3) | 0x30);
      trig_cfg.value = m_adapter->read(m_base_addr + TDCR);
      break;
  }
  return trig_cfg;
}
