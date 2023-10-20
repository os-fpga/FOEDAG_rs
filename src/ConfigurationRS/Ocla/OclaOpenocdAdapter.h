#ifndef __OCLAOPENOCDADAPTER_H__
#define __OCLAOPENOCDADAPTER_H__

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>

#include "OclaJtagAdapter.h"

using ExecFuncType = std::function<int(const std::string&, std::string&,
                                       std::ostream*, std::atomic<bool>&)>;

class OclaOpenocdAdapter : public OclaJtagAdapter {
 public:
  OclaOpenocdAdapter(std::string filepath, ExecFuncType cmdexec);
  virtual ~OclaOpenocdAdapter();
  virtual void write(uint32_t addr, uint32_t data);
  virtual uint32_t read(uint32_t addr);
  virtual std::vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                     uint32_t increase_by = 0);
  virtual void set_target_device(Device device, std::vector<Tap> taplist);

 private:
  std::string convert_transport_to_string(TransportType transport,
                                          std::string defval = "jtag");
  std::string build_command(const std::string& cmd);
  int execute_command(const std::string& cmd, std::string& output);
  std::vector<uint32_t> parse(const std::string& output);
  std::string m_filepath;
  ExecFuncType m_cmdexec;
  Device m_device;
  std::vector<Tap> m_taplist;
};

#endif  //__OCLAOPENOCDADAPTER_H__
