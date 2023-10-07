#ifndef __OCLAJTAGADAPTER_H__
#define __OCLAJTAGADAPTER_H__

#include <cstdint>
#include <vector>

class OclaJtagAdapter {
 public:
  virtual ~OclaJtagAdapter(){};
  virtual void write(uint32_t addr, uint32_t data) = 0;
  virtual uint32_t read(uint32_t addr) = 0;
  virtual std::vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                     uint32_t increase_by = 0) = 0;
};

#endif  //__OCLAJTAGADAPTER_H__
