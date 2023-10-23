#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <cstdint>
#include <string>

#include "Cable.h"
#include "Tap.h"

enum DeviceType { GEMINI, OCLA, OEM /* for all non-RS devices */ };

struct Device {
  uint32_t index;
  std::string name;
  DeviceType type;
  Cable cable;
  Tap tap;
};

#endif  // __DEVICE_H__
