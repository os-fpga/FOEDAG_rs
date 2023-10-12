#ifndef __FSTWAVEFORMWRITER_H__
#define __FSTWAVEFORMWRITER_H__

#include "WaveformWriter.h"

class FstWaveformWriter : public WaveformWriter {
 public:
  virtual void write(vector<uint32_t> values, string filepath);
};

#endif  //__FSTWAVEFORMWRITER_H__
