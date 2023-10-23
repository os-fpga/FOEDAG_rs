#ifndef __HARDWAREMANAGER_H__
#define __HARDWAREMANAGER_H__

#include <iostream>
#include <stdexcept>

#include "Device.h"
#include "JtagAdapter.h"
#include "Tap.h"

struct HardwareManager_CABLE_INFO {
  std::string name;
  CableType type;
  uint16_t vid;
  uint16_t pid;
};

struct HardwareManager_DEVICE_INFO {
  std::string name;
  uint32_t idcode;
  uint32_t irlength;
  uint32_t irmask;
  DeviceType type;
};

class HardwareManager {
 public:
  HardwareManager(JtagAdapter *m_adapter);
  virtual ~HardwareManager();
  std::vector<Tap> get_taps(const Cable &cable);
  std::vector<Cable> get_cables();
  bool is_cable_exists(uint32_t cable_index);
  bool is_cable_exists(std::string cable_name,
                       bool numeric_name_as_index = false);
  std::vector<Device> get_devices();
  std::vector<Device> get_devices(const Cable &cable);
  std::vector<Device> get_devices(uint32_t cable_index);
  std::vector<Device> get_devices(std::string cable_name,
                                  bool numeric_name_as_index = false);
  bool find_device(std::string cable_name, uint32_t device_index,
                   Device &device, std::vector<Tap> &taplist,
                   bool numeric_name_as_index = false);

 private:
  static const std::vector<HardwareManager_CABLE_INFO> m_cable_db;
  static const std::vector<HardwareManager_DEVICE_INFO> m_device_db;
  JtagAdapter *m_adapter;
};

#endif  //__HARDWAREMANAGER_H__
