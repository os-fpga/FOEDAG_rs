#ifndef BITASSEMBLER_OCLA_H
#define BITASSEMBLER_OCLA_H

#include <fstream>
#include <iostream>
#include <string>

#include "CFGObject/CFGObject_auto.h"
#include "nlohmann_json/json.hpp"

class BitAssembler_OCLA {
 public:
  static void parse(CFGObject_BITOBJ& bitobj, const std::string& taskPath,
                    const std::string& yosysBin,
                    const std::string& analyzeCMDPath);

 private:
  static void extract_ocla_info(CFGObject_BITOBJ& bitobj, nlohmann::json& json);
  static bool validate_ocla(nlohmann::json& ocla);
  static bool validate_ocla_debug_subsystem(
      nlohmann::json& ocla_debug_subsystem);
};

#endif