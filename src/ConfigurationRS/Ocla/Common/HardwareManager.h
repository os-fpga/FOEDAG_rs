#ifndef __HARDWAREMANAGER_H__
#define __HARDWAREMANAGER_H__

#include <iostream>
#include <stdexcept>

#include "JtagAdapter.h"

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
  std::vector<Cable> get_cables();
  std::vector<Tap> get_taps(const Cable &cable);
  std::vector<Device> get_devices(const Cable &cable,
                                  std::vector<Tap> *output = nullptr);
  void set_device_db(std::vector<HardwareManager_DEVICE_INFO> device_db) {
    m_device_db = device_db;
  };

 private:
  static const std::vector<HardwareManager_CABLE_INFO> m_cable_db;
  std::vector<HardwareManager_DEVICE_INFO> m_device_db;
  JtagAdapter *m_adapter;
};

#endif  //__HARDWAREMANAGER_H__
