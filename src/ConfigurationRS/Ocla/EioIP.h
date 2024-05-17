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

// CTRL (Control register) bitfield definitions
#define EIO_CTRL_PRS_Pos (0)
#define EIO_CTRL_PRS_Width (1)
#define EIO_CTRL_PRS_Msk (((1u << EIO_CTRL_PRS_Width) - 1) << EIO_CTRL_PRS_Pos)

class OclaJtagAdapter;

enum eio_prs_mode { PROBE_IN = 0, PROBE_OUT = 1 };

class EioIP {
 public:
  EioIP(OclaJtagAdapter *adapter, uint32_t baseaddr);
  ~EioIP();
  std::string get_type() const;
  uint32_t get_version() const;
  uint32_t get_id() const;
  eio_prs_mode get_prs_mode() const;
  void set_prs_mode(eio_prs_mode mode);
  void write_output_bits(std::vector<uint32_t> values, uint32_t length);
  std::vector<uint32_t> readback_output_bits(uint32_t length);
  std::vector<uint32_t> read_input_bits(uint32_t length);

 private:
  std::vector<uint32_t> read_bits(uint32_t addr, uint32_t length);
  void read_registers();
  OclaJtagAdapter *m_adapter;
  uint32_t m_baseaddr;
  uint32_t m_ctrl;
  uint32_t m_type;
  uint32_t m_version;
  uint32_t m_id;
};

#endif  //__EIOIP_H__
