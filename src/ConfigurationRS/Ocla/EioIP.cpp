#include "EioIP.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaJtagAdapter.h"

#define MAX_IO_INPUT_REG (2)
#define MAX_IO_OUTPUT_REG (2)

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

void EioIP::write_output_bits(std::vector<uint32_t> values, uint32_t length) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(length > 0);
  CFG_ASSERT(length <= MAX_IO_OUTPUT_REG);
  CFG_ASSERT(values.size() >= length);
  for (uint32_t i = 0; i < length; i++) {
    m_adapter->write(m_baseaddr + EIO_AXI_DAT_OUT + (i << 2), values[i]);
  }
}

std::vector<uint32_t> EioIP::read_input_bits(uint32_t length) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(length > 0);
  CFG_ASSERT(length <= MAX_IO_INPUT_REG);
  std::vector<uint32_t> values{};
  auto result = m_adapter->read(m_baseaddr + EIO_AXI_DAT_IN, length, 4);
  for (auto &r : result) {
    values.push_back(r.data);
  }
  return values;
}

void EioIP::read_registers() {
  CFG_ASSERT(m_adapter != nullptr);
  m_ctrl = m_adapter->read(m_baseaddr + EIO_CTRL);
  m_type = m_adapter->read(m_baseaddr + EIO_IP_TYPE);
  m_version = m_adapter->read(m_baseaddr + EIO_IP_VERSION);
  m_id = m_adapter->read(m_baseaddr + EIO_IP_ID);
}
