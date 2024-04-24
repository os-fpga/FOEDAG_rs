#include "EioIP.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaJtagAdapter.h"

EioIP::EioIP(OclaJtagAdapter *adapter, uint32_t baseaddr)
    : m_adapter(adapter),
      m_baseaddr(baseaddr),
      m_ctrl(0),
      m_type(0),
      m_version(0),
      m_id(0) {
  read_registers();
}

EioIP::~EioIP() {}

std::string EioIP::get_type() const {
  char buffer[10];
  uint32_t type = CFG_reverse_byte_order_u32(m_type << 8);
  snprintf(buffer, sizeof(buffer), "%.*s", 4, (char *)&type);
  return std::string(buffer);
}

uint32_t EioIP::get_version() const { return m_version; }

uint32_t EioIP::get_id() const { return m_id; }

void EioIP::write(uint32_t value, eio_probe_register_t reg) {
  CFG_ASSERT(m_adapter != nullptr);
  m_adapter->write(m_baseaddr + EIO_AXI_DAT_OUT + uint32_t(reg), value);
}

uint32_t EioIP::read(eio_probe_register_t reg) {
  return m_adapter->read(m_baseaddr + EIO_AXI_DAT_IN + uint32_t(reg));
}

void EioIP::read_registers() {
  CFG_ASSERT(m_adapter != nullptr);
  m_ctrl = m_adapter->read(m_baseaddr + EIO_CTRL);
  m_type = m_adapter->read(m_baseaddr + EIO_IP_TYPE);
  m_version = m_adapter->read(m_baseaddr + EIO_IP_VERSION);
  m_id = m_adapter->read(m_baseaddr + EIO_IP_ID);
}
