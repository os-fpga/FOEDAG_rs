#ifndef __OCLAOPENOCDADAPTER_H__
#define __OCLAOPENOCDADAPTER_H__

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <tuple>

#include "Configuration/HardwareManager/OpenocdAdapter.h"
#include "OclaJtagAdapter.h"

class OclaOpenocdAdapter : public OclaJtagAdapter,
                           public FOEDAG::OpenocdAdapter {
 public:
  OclaOpenocdAdapter(std::string openocd);
  virtual ~OclaOpenocdAdapter();
  virtual void write(uint32_t addr, uint32_t data);
  virtual uint32_t read(uint32_t addr);
  virtual std::vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                     uint32_t increase_by = 0);
  virtual void set_target_device(FOEDAG::Device device,
                                 std::vector<FOEDAG::Tap> taplist);

 private:
  int execute_command(const std::string& cmd, std::string& output);
  std::string build_tcl_proc(uint32_t tap_num);
  std::vector<std::tuple<uint32_t, uint32_t>> parse(const std::string& output);
  std::string m_openocd;
  FOEDAG::Device m_device;
  std::vector<FOEDAG::Tap> m_taplist;
};

#endif  //__OCLAOPENOCDADAPTER_H__
