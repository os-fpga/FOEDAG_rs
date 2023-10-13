#ifndef __FSTWAVEFORMWRITER_H__
#define __FSTWAVEFORMWRITER_H__

#include "OclaWaveformWriter.h"

class FstWaveformWriter : public WaveformWriter {
 public:
  virtual void write(std::vector<uint32_t> values, std::string filepath);

 private:
  uint32_t countTotalBitwidth();
};

#endif  //__FSTWAVEFORMWRITER_H__
