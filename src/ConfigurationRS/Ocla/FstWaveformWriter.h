#ifndef __FSTWAVEFORMWRITER_H__
#define __FSTWAVEFORMWRITER_H__

#include "OclaWaveformWriter.h"

class FstWaveformWriter : public OclaWaveformWriter {
 public:
  virtual void write(std::vector<uint32_t> values, std::string filepath);

 private:
  uint32_t count_total_bitwidth();
};

#endif  //__FSTWAVEFORMWRITER_H__
