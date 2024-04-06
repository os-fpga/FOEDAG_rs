#ifndef __OCLAFSTWAVEFORMWRITER_H__
#define __OCLAFSTWAVEFORMWRITER_H__

#include "Ocla.h"

class OclaFstWaveformWriter {
 public:
  bool write(oc_waveform_t &waveform, std::string filepath);
};

#endif  //__OCLAFSTWAVEFORMWRITER_H__
