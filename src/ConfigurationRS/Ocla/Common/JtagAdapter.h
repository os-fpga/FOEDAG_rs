#ifndef __JTAGADAPTER_H__
#define __JTAGADAPTER_H__

#include <cstdint>
#include <vector>

#include "Cable.h"

class JtagAdapter {
 public:
  virtual std::vector<uint32_t> scan(const Cable &cable) = 0;
};

#endif  //__JTAGADAPTER_H__
