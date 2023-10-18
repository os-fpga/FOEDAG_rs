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
  virtual ~OpenocdJtagAdapter();
  virtual void write(uint32_t addr, uint32_t data);
  virtual uint32_t read(uint32_t addr);
  virtual std::vector<uint32_t> read(uint32_t base_addr, uint32_t num_reads,
                                     uint32_t increase_by = 0);
  virtual std::vector<Tap> get_taps(const Cable& cable);
  virtual void set_cable(Cable* cable) { m_cable = cable; };
  virtual void set_device(Device* device) { m_device = device; };
  virtual void set_taps(std::vector<Tap> taps) { m_taps = taps; };

 private:
  std::string build_command(const std::string& cmd, const Cable* cable,
                            const Device* device, const std::vector<Tap> taps);
  int execute_command(const std::string& cmd, std::string& output,
                      const Cable* cable, const Device* device = nullptr,
                      const std::vector<Tap> taps = {});
  std::vector<uint32_t> parse(const std::string& output);
  std::string m_filepath;
  ExecFuncType m_cmdexec;
  Cable* m_cable;
  Device* m_device;
  std::vector<Tap> m_taps;
};

#endif  //__OPENOCDJTAGADAPTER_H__
