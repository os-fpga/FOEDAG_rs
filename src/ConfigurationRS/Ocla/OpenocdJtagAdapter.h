#ifndef __OPENOCDJTAGADAPTER_H__
#define __OPENOCDJTAGADAPTER_H__

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>

#include "JtagAdapter.h"

using namespace std;

class Cable;

using ExecFuncType =
    function<int(const string&, string&, ostream*, atomic<bool>&)>;

class OpenocdJtagAdapter : public JtagAdapter {
 public:
  OpenocdJtagAdapter(string filepath, ExecFuncType cmdexec,
                     Cable* cable = nullptr);
  virtual ~OpenocdJtagAdapter(){};
  virtual void write(uint32_t addr, uint32_t data);
  virtual uint32_t read(uint32_t addr);
  virtual vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                uint32_t increase_by = 0);
  void setId(uint32_t id);
  void setSpeedKhz(uint32_t speedKhz);

 private:
  stringstream buildCommand(const string& cmd);
  int executeCommand(const string& cmd, string& output);
  vector<uint32_t> parse(const string& output);
  uint32_t m_irlen;
  uint32_t m_id;
  uint32_t m_speedKhz;
  string m_filepath;
  ExecFuncType m_cmdexec = nullptr;
  Cable* m_cable = nullptr;
};

#endif  //__OPENOCDJTAGADAPTER_H__
