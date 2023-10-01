#ifndef __JTAGADAPTER_H__
#define __JTAGADAPTER_H__

#include <cstdint>
#include <vector>

using namespace std;

class JtagAdapter {
 public:
  virtual ~JtagAdapter(){};
  virtual void write(uint32_t addr, uint32_t data) = 0;
  virtual uint32_t read(uint32_t addr) = 0;
  virtual vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                uint32_t increase_by = 0) = 0;
};

#endif  //__JTAGADAPTER_H__
