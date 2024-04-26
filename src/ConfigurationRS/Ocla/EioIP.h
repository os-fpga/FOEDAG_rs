#ifndef __EIOIP_H__
#define __EIOIP_H__

#include <cstdint>
#include <string>
#include <vector>

#define EIO_CTRL (0x00)        // [RW] Control register (Reserved)
#define EIO_IP_TYPE (0x04)     // [RO] IP Type Register ("EIO")
#define EIO_IP_VERSION (0x08)  // [RO] Define version of IP
#define EIO_IP_ID (0x0C)       // [RO] Unique identifier for IP
#define EIO_AXI_DAT_IN \
  (0x10)  // [RO] Data at the input probes of EIO (received from the DUT)
#define EIO_AXI_DAT_OUT \
  (0x10)  // [WO] Data at the output probes of EIO (sent to the DUT)

class OclaJtagAdapter;

class EioIP {
 public:
  EioIP(OclaJtagAdapter *adapter, uint32_t baseaddr);
  ~EioIP();
  std::string get_type() const;
  uint32_t get_version() const;
  uint32_t get_id() const;
  void write(std::vector<uint32_t> values, uint32_t length);
  std::vector<uint32_t> read(uint32_t length);

 private:
  void read_registers();
  OclaJtagAdapter *m_adapter;
  uint32_t m_baseaddr;
  uint32_t m_ctrl;
  uint32_t m_type;
  uint32_t m_version;
  uint32_t m_id;
};

#endif  //__EIOIP_H__
