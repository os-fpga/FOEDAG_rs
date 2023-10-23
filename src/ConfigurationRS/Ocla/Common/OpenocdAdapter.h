#ifndef __OPENOCDADAPTER_H__
#define __OPENOCDADAPTER_H__

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "JtagAdapter.h"

using CommandExecutorFuncType = std::function<int(
    const std::string &, std::string &, std::ostream *, std::atomic<bool> &)>;

class OpenocdAdapter : public JtagAdapter {
 public:
  OpenocdAdapter(std::string openocd_filepath,
                 CommandExecutorFuncType command_executor)
      : m_openocd_filepath(openocd_filepath),
        m_command_executor(command_executor) {}
  virtual std::vector<uint32_t> scan(const Cable &cable);

 private:
  int execute(const Cable &cable, std::string &output);
  std::string convert_transport_to_string(TransportType transport,
                                          std::string defval = "jtag");
  std::string m_openocd_filepath;
  CommandExecutorFuncType m_command_executor;
};

#endif  //__OPENOCDADAPTER_H__
