#ifndef __CABLE_H__
#define __CABLE_H__

#include <cstdint>
#include <string>

enum TransportType { JTAG = 1 };

enum CableType { FTDI, JLINK };

struct Cable {
  uint32_t index;
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t bus_addr;
  uint8_t port_addr;
  uint8_t device_addr;
  uint16_t channel;
  uint32_t speed;
  std::string serial_number;
  std::string description;
  std::string name;
  TransportType transport;
  CableType cable_type;
};

#endif  // __CABLE_H__
