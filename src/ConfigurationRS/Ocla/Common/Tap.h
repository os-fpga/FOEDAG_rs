#ifndef __TAP_H__
#define __TAP_H__

#include <cstdint>

struct Tap {
  uint32_t index;
  uint32_t idcode;
  uint32_t irlength;
};

#endif  // __TAP_H__
