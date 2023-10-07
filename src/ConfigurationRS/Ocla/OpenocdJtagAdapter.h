#ifndef __OPENOCDJTAGADAPTER_H__
#define __OPENOCDJTAGADAPTER_H__

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>

#include "OclaJtagAdapter.h"

using ExecFuncType = std::function<int(const std::string&, std::string&,
                                       std::ostream*, std::atomic<bool>&)>;

class OpenocdJtagAdapter : public OclaJtagAdapter {
 public:
  OpenocdJtagAdapter(std::string filepath, ExecFuncType cmdexec);
  virtual ~OpenocdJtagAdapter(){};
  virtual void write(uint32_t addr, uint32_t data);
  virtual uint32_t read(uint32_t addr);
  virtual std::vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                     uint32_t increase_by = 0);
  void setId(uint32_t id);
  void setSpeedKhz(uint32_t speedKhz);

 private:
  std::stringstream buildCommand(const std::string& cmd);
  int executeCommand(const std::string& cmd, std::string& output);
  std::vector<uint32_t> parse(const std::string& output);
  std::string m_filepath;
  ExecFuncType m_cmdexec = nullptr;
  uint32_t m_id;
  uint32_t m_irlen;
  uint32_t m_speedKhz;
};

#endif  //__OPENOCDJTAGADAPTER_H__
