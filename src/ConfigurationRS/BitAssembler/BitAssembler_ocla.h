#ifndef BITASSEMBLER_OCLA_H
#define BITASSEMBLER_OCLA_H

#include <fstream>
#include <iostream>
#include <string>

#include "CFGObject/CFGObject_auto.h"
#include "nlohmann_json/json.hpp"

struct BitAssembler_IP_MODULE;
struct BitAssembler_OCLA_MODULE;
struct BitAssembler_AXIL_MODULE;
struct BitAssembler_OCLA_INSTANTIATION;
struct BitAssembler_OCLA_INSTANCE;
struct BitAssembler_OCLA_PROBE;
struct BitAssembler_OCLA_REPORT {
  BitAssembler_OCLA_REPORT() {}
  BitAssembler_OCLA_REPORT(std::string filepath);
  void write(std::string msg);
  void close();
  std::ofstream stdout;
};

class BitAssembler_OCLA {
 public:
  static void parse(CFGObject_BITOBJ& bitobj, const std::string& taskPath,
                    const std::string& yosysBin, const std::string& hierPath,
                    const std::string& ysPath);

 private:
  template <typename T>
  static void get_ip_wrapper_module(nlohmann::json& hier,
                                    const std::string& ip_name,
                                    const uint32_t ip_type,
                                    std::vector<T*>& ocla_modules,
                                    BitAssembler_OCLA_REPORT& report);
  static bool get_module_instantiations(BitAssembler_IP_MODULE*& ip,
                                        nlohmann::json& hier);
  static bool get_module_instantiations(BitAssembler_IP_MODULE*& ip,
                                        nlohmann::json& module, bool top,
                                        std::string module_name,
                                        std::string module_key,
                                        nlohmann::json& fileIDs);
  static bool get_module_instantiations(BitAssembler_IP_MODULE*& ip,
                                        std::string language,
                                        std::string instantiator_name,
                                        std::string instantiator_module,
                                        bool top, nlohmann::json& moduleInsts,
                                        nlohmann::json& fileIDs);
  static bool check_wrapper_modules(
      std::vector<BitAssembler_OCLA_MODULE*>& oclas,
      std::vector<BitAssembler_AXIL_MODULE*>& axils);
  static bool generate_full_rtlil(const std::string& yosysBin,
                                  const std::string& ysPath,
                                  const std::string& taskPath,
                                  const std::string& rtlilPath);
  static bool generate_rtlil(
      const std::vector<std::pair<std::string, std::string>>& cells,
      const std::string& yosysBin, const std::string& ysPath,
      const std::string& taskPath, const std::string& rtlilPath);
  static bool get_modules(
      const std::vector<std::string>& modules,
      std::vector<std::pair<std::string, std::string>>& cells,
      const std::string& rtlilPath, BitAssembler_OCLA_REPORT& report);
  static bool get_probes(const std::vector<std::string>& modules,
                         std::vector<BitAssembler_OCLA_INSTANCE*>& instances,
                         const std::string& rtlilPath,
                         const std::string& module,
                         const std::string& module_instance,
                         uint32_t& connection_start_line,
                         BitAssembler_OCLA_REPORT& report);
  static bool get_probe_size(
      const size_t start_line, const std::string& rtlilPath,
      std::vector<std::string>& signals,
      std::vector<std::vector<BitAssembler_OCLA_PROBE*>>& signal_ptrs,
      BitAssembler_OCLA_REPORT& report);
  static bool match_instance(BitAssembler_OCLA_INSTANCE*& instance,
                             std::vector<BitAssembler_OCLA_MODULE*>& oclas,
                             BitAssembler_OCLA_REPORT& report);
  static bool get_connection(const std::string& rtlilPath,
                             BitAssembler_OCLA_INSTANCE*& instance,
                             BitAssembler_AXIL_MODULE*& axil_instance,
                             std::vector<std::string>& axil_connections,
                             std::vector<uint32_t>& found_axil_connections,
                             const std::string& source, uint32_t start_line,
                             uint32_t level, BitAssembler_OCLA_REPORT& report);
};

#endif