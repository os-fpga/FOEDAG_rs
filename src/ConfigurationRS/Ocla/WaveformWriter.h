#ifndef __WAVEFORMWRITER_H__
#define __WAVEFORMWRITER_H__

#include <cstdint>
#include <iostream>
#include <vector>

struct signal_info {
  std::string name;
  uint32_t bitwidth;
};

class WaveformWriter {
 protected:
  std::vector<signal_info> m_signals;
  uint32_t m_width = 0;
  uint32_t m_depth = 0;

 public:
  virtual ~WaveformWriter(){};
  virtual void write(std::vector<uint32_t> values, std::string filepath) = 0;
  virtual void setWidth(uint32_t width) { m_width = width; }
  virtual void setDepth(uint32_t depth) { m_depth = depth; }
  virtual void setSignals(std::vector<signal_info> sigs) { m_signals = sigs; }
};

#endif  //__WAVEFORMWRITER_H__
