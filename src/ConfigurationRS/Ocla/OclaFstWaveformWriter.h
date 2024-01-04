#ifndef __OCLAFSTWAVEFORMWRITER_H__
#define __OCLAFSTWAVEFORMWRITER_H__

#include "OclaWaveformWriter.h"

class OclaFstWaveformWriter : public OclaWaveformWriter {
 public:
  virtual void write(std::vector<uint32_t> values, std::string filepath);

 private:
  uint32_t count_total_bitwidth();
};

#endif  //__OCLAFSTWAVEFORMWRITER_H__
