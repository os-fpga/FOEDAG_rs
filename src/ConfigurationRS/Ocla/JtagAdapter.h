#ifndef __JTAGADAPTER_H__
#define __JTAGADAPTER_H__

#include <cstdint>
#include <string>
#include <vector>

enum TransportType { JTAG = 1 };

enum DeviceType { GEMINI, OCLA, OEM /* for all non-RS devices */ };

enum CableType { FTDI, JLINK };

struct Tap {
  uint32_t index;
  uint32_t idcode;
  uint32_t irlength;
};

struct Device {
  uint32_t index;
  std::string name;
  DeviceType type;
  Tap tap;
};

struct Cable {
  uint32_t index;
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t bus_addr;
  uint8_t port_addr;
  uint8_t device_addr;
  uint16_t channel = 0;
  uint32_t speed;
  std::string serial_number;
  std::string description;
  std::string name;
  TransportType transport;
  CableType cable_type;
};

class JtagAdapter {
 public:
  virtual std::vector<Tap> get_taps(Cable &cable) = 0;
};

#endif  //__JTAGADAPTER_H__
