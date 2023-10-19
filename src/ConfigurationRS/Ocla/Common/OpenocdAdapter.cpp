#include "OpenocdAdapter.h"

#include <filesystem>
#include <regex>
#include <sstream>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"

std::vector<uint32_t> OpenocdAdapter::scan(const Cable &cable) {
  std::vector<uint32_t> idcode_array;
  std::string line;
  std::cmatch matches;
  std::string output;

  const std::string pattern(
      R"((\d+) +(\w+.\w+) +([YN]) +(0x[0-9a-f]+) +(0x[0-9a-f]+) +(\d+) +(0x[0-9a-f]+) +(0x[0-9a-f]+))");

  // OpenOCD "scan_chain" command output text example:-
  //    TapName            Enabled IdCode     Expected   IrLen IrCap IrMask
  // -- ------------------ ------- ---------- ---------- ----- ----- ------
  //  0 omap5912.dsp          Y    0x03df1d81 0x03df1d81    38 0x01  0x03
  //  1 omap5912.arm          Y    0x0692602f 0x0692602f     4 0x01  0x0f
  //  2 omap5912.unknown      Y    0x00000000 0x00000000     8 0x01  0x03
  //  3 auto0.tap             Y    0x20000913 0x00000000     5 0x01  0x03

  CFG_ASSERT_MSG(execute(cable, output) == 0, "cmdexec error: %s",
                 output.c_str());
  std::stringstream ss(output);

  while (std::getline(ss, line)) {
    if (std::regex_search(line.c_str(), matches,
                          std::regex(pattern, std::regex::icase)) == true) {
      uint32_t idcode = (uint32_t)CFG_convert_string_to_u64(matches[4]);
      idcode_array.push_back(idcode);
    }
  }

  return idcode_array;
}

int OpenocdAdapter::execute(const Cable &cable, std::string &output) {
  std::atomic<bool> stop = false;
  std::ostringstream ss;

  CFG_ASSERT(std::filesystem::exists(m_openocd_filepath));
  CFG_ASSERT(m_command_executor != nullptr);

  ss << " -l /dev/stdout"  //<-- not windows friendly
     << " -d2";

  // setup cable configuration
  if (cable.cable_type == FTDI) {
    ss << " -c \"adapter driver ftdi;"
       << "ftdi vid_pid " << std::hex << std::showbase << cable.vendor_id << " "
       << cable.product_id << ";" << std::noshowbase << std::dec
       << "ftdi layout_init 0x0c08 0x0f1b;";
  } else if (cable.cable_type == JLINK) {
    ss << " -c \"adapter driver jlink;";
  }

  // setup general cable configuration
  ss << "adapter speed " << cable.speed << ";"
     << "transport select jtag;"
     << "telnet_port disabled;"
     << "gdb_port disabled;\"";

  // use "scan_chain" cmd to collect tap ids
  ss << " -c \"init\"";
  ss << " -c \"scan_chain\"";
  ss << " -c \"exit\"";

  // run the command
  int res = m_command_executor(
      "OPENOCD_DEBUG_LEVEL=-3 " + m_openocd_filepath + ss.str(), output,
      nullptr, stop);
  return res;
}
