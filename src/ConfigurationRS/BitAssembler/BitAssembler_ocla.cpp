#include "BitAssembler_ocla.h"

void BitAssembler_OCLA::parse(CFGObject_BITOBJ& bitobj,
                              const std::string& taskPath,
                              const std::string& yosysBin,
                              const std::string& analyzeCMDPath) {
  if (std::filesystem::exists(yosysBin) &&
      std::filesystem::exists(analyzeCMDPath)) {
    CFG_POST_MSG("  OCLA Parser");
    CFG_POST_MSG("    Analyze CMD path: %s", analyzeCMDPath.c_str());
    // Read text line by line
    std::ifstream file(analyzeCMDPath.c_str());
    CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", analyzeCMDPath.c_str());
    std::string outPath = CFG_print("%s/ocla.ys", taskPath.c_str());
    CFG_POST_MSG("    OCLA Analyze CMD path: %s", outPath.c_str());
    std::ofstream outfile(outPath.c_str());
    CFG_ASSERT_MSG(outfile.is_open(), "Fail to open %s for writing",
                   outPath.c_str());
    std::string line = "";
    while (getline(file, line)) {
      outfile << line.c_str() << "\n";
    }
    outfile << "\n# Start OCLA Analyzer\n";
    outfile << CFG_print("ocla_analyze -file %s/ocla.json\n", taskPath.c_str())
                   .c_str();
    file.close();
    outfile.close();
    // Execute yosys
    std::string cmd = CFG_print("%s %s", yosysBin.c_str(), outPath.c_str());
    std::string cmd_output = "";
    std::atomic<bool> stop = false;
    if (CFG_execute_cmd(cmd, cmd_output, nullptr, stop) == 0) {
      std::string ocla_json = CFG_print("%s/ocla.json", taskPath.c_str());
      CFG_POST_MSG("    OCLA JSON: %s", ocla_json.c_str());
      if (std::filesystem::exists(ocla_json)) {
        std::ifstream jsonfile(ocla_json.c_str());
        CFG_ASSERT(jsonfile.is_open() && jsonfile.good());
        nlohmann::json json = nlohmann::json::parse(jsonfile);
        extract_ocla_info(bitobj, json);
        jsonfile.close();
      } else {
        CFG_POST_WARNING("Could not find expected output \"%s\"",
                         ocla_json.c_str());
      }
    } else {
      CFG_POST_WARNING("Fail to run command \"%s\"", cmd.c_str());
    }
  }
}

void BitAssembler_OCLA::extract_ocla_info(CFGObject_BITOBJ& bitobj,
                                          nlohmann::json& json) {
  CFG_ASSERT(!bitobj.check_exist("ocla"));
  nlohmann::json ocla;
  nlohmann::json ocla_debug_subsystem;
  std::string json_string = "";
  if (!json.is_object()) {
    CFG_POST_WARNING("JSON is in invalid format");
    goto EXTRACT_OCLA_INFO_END;
  }
  if (!json.contains("messages")) {
    CFG_POST_WARNING("Design does not contain messages");
    goto EXTRACT_OCLA_INFO_END;
  }
  if (!json.contains("ocla")) {
    CFG_POST_WARNING("Design does not contain OCLA information");
    goto EXTRACT_OCLA_INFO_END;
  }
  if (!json.contains("ocla_debug_subsystem")) {
    CFG_POST_WARNING(
        "Design does not contain OCLA Debug Subsystem information");
    goto EXTRACT_OCLA_INFO_END;
  }
  if (json.size() != 3) {
    CFG_POST_WARNING(
        "JSON contains more that three expected entires: messages, OCLA, OCLA "
        "Debug Subsystem");
    goto EXTRACT_OCLA_INFO_END;
  }
  ocla = json["ocla"];
  ocla_debug_subsystem = json["ocla_debug_subsystem"];
  if (!ocla.is_array()) {
    CFG_POST_WARNING("JSON OCLA is not an array");
    goto EXTRACT_OCLA_INFO_END;
  }
  if (!ocla_debug_subsystem.is_object()) {
    CFG_POST_WARNING("JSON OCLA Debug Subsystem is not an object");
    goto EXTRACT_OCLA_INFO_END;
  }
  // Loop through each
  CFG_POST_MSG("    Detected %d OCLA IP(s)", ocla.size());
  for (nlohmann::json& o : ocla) {
    if (!validate_ocla(o)) {
      CFG_POST_WARNING("Invalidate entire OCLA design");
      goto EXTRACT_OCLA_INFO_END;
    }
  }
  if (!validate_ocla_debug_subsystem(ocla_debug_subsystem)) {
    CFG_POST_WARNING("Invalidate entire OCLA design");
    goto EXTRACT_OCLA_INFO_END;
  }
  for (nlohmann::json& o : ocla) {
    CFG_POST_MSG("      OCLA IP:");
    CFG_POST_MSG("        TYPE: %s, Version: 0x%08X, ID: 0x%08X",
                 std::string(o["IP_TYPE"]).c_str(), (uint32_t)(o["IP_VERSION"]),
                 (uint32_t)(o["IP_ID"]));
    CFG_POST_MSG("        Base Addr: 0x%08X", (uint32_t)(o["addr"]));
    CFG_POST_MSG("        Probes: %d", (uint32_t)(o["NO_OF_PROBES"]));
    for (auto& p : o["probes"]) {
      CFG_POST_MSG("          -> %s", std::string(p).c_str());
    }
  }
  json.erase("messages");
  json_string = json.dump();
  bitobj.write_str("ocla", json_string);
  CFG_ASSERT(bitobj.check_exist("ocla"));
EXTRACT_OCLA_INFO_END:
  return;
}

bool BitAssembler_OCLA::validate_ocla(nlohmann::json& ocla) {
  bool status = false;
  const std::vector<std::string> params = {
      "IP_VERSION",   "IP_ID",     "AXI_ADDR_WIDTH", "AXI_DATA_WIDTH",
      "NO_OF_PROBES", "MEM_DEPTH", "INDEX"};
  if (!ocla.is_object()) {
    CFG_POST_WARNING("JSON sub-OCLA is not an object");
    goto VALIDATE_OCLA_END;
  }
  if (!ocla.contains("IP_TYPE") || !ocla["IP_TYPE"].is_string()) {
    CFG_POST_WARNING(
        "JSON sub-OCLA does not have parameter IP_TYPE or parameter type is "
        "not string");
    goto VALIDATE_OCLA_END;
  }
  if (std::string(ocla["IP_TYPE"]) != "ocla") {
    CFG_POST_WARNING("JSON sub-OCLA IP_TYPE is not \"ocla\". Found %s",
                     std::string(ocla["IP_TYPE"]).c_str());
    goto VALIDATE_OCLA_END;
  }
  for (auto p : params) {
    if (!ocla.contains(p) ||
        !(ocla[p].is_number_integer() || ocla[p].is_number_unsigned())) {
      CFG_POST_WARNING(
          "JSON sub-OCLA does not have parameter %s or the parameter type is "
          "not number",
          p.c_str());
      goto VALIDATE_OCLA_END;
    }
  }
  if (!ocla.contains("addr") || !(ocla["addr"].is_number_integer() ||
                                  ocla["addr"].is_number_unsigned())) {
    CFG_POST_WARNING(
        "JSON sub-OCLA does not have addr information or the information type "
        "is not number");
    goto VALIDATE_OCLA_END;
  }
  if (!ocla.contains("probes") || !ocla["probes"].is_array()) {
    CFG_POST_WARNING(
        "JSON sub-OCLA does not have probes information or the information "
        "type "
        "is not an array");
    goto VALIDATE_OCLA_END;
  }
  for (auto& probe : ocla["probes"]) {
    if (!probe.is_string()) {
      CFG_POST_WARNING("JSON sub-OCLA probe signal is not a string");
      goto VALIDATE_OCLA_END;
    }
  }
  // Type param, params, addr (not param), probes (not param)
  if (ocla.size() != (1 + params.size() + 2)) {
    CFG_POST_WARNING(
        "JSON sub-OCLA has extra object key. Expected %d count, found %d",
        1 + params.size() + 2, ocla.size());
    goto VALIDATE_OCLA_END;
  }
  status = true;
VALIDATE_OCLA_END:
  return status;
}

bool BitAssembler_OCLA::validate_ocla_debug_subsystem(
    nlohmann::json& ocla_debug_subsystem) {
  bool status = false;
  std::vector<std::string> params = {"IP_VERSION", "IP_ID",
                                     "CORES",      "PROBES_SUM",
                                     "No_AXI_Bus", "AXI_Core_BaseAddress"};
  std::vector<std::string> str_params = {"MODE", "AXI_TYPE"};
  for (uint32_t i = 0; i < 15; i++) {
    params.push_back(CFG_print("Probe%d_Width", i + 1));
    params.push_back(CFG_print("IF%d_BaseAddress", i + 1));
    params.push_back(CFG_print("IF%d_Probes", i + 1));
  }
  if (!ocla_debug_subsystem.contains("IP_TYPE") ||
      !ocla_debug_subsystem["IP_TYPE"].is_string()) {
    CFG_POST_WARNING(
        "JSON OCLA Debug Subsystem does not have parameter IP_TYPE or "
        "parameter type is "
        "not string");
    goto VALIDATE_OCLA_DEBUG_SUBSYSTEM_END;
  }
  if (std::string(ocla_debug_subsystem["IP_TYPE"]) != "ocla") {
    CFG_POST_WARNING(
        "JSON OCLA Debug Subsystem IP_TYPE is not \"ocla\". Found %s",
        std::string(ocla_debug_subsystem["IP_TYPE"]).c_str());
    goto VALIDATE_OCLA_DEBUG_SUBSYSTEM_END;
  }
  for (auto p : params) {
    if (!ocla_debug_subsystem.contains(p) ||
        !(ocla_debug_subsystem[p].is_number_integer() ||
          ocla_debug_subsystem[p].is_number_unsigned())) {
      CFG_POST_WARNING(
          "JSON OCLA Debug Subsystem does not have parameter %s or the "
          "parameter type is not number",
          p.c_str());
      goto VALIDATE_OCLA_DEBUG_SUBSYSTEM_END;
    }
  }
  for (auto p : str_params) {
    if (!ocla_debug_subsystem.contains(p) ||
        !ocla_debug_subsystem[p].is_string()) {
      CFG_POST_WARNING(
          "JSON OCLA Debug Subsystem does not have parameter %s or the "
          "parameter type is not string",
          p.c_str());
      goto VALIDATE_OCLA_DEBUG_SUBSYSTEM_END;
    }
  }
  // Type param, params, addr (not param), probes (not param)
  if (ocla_debug_subsystem.size() != (1 + params.size() + str_params.size())) {
    CFG_POST_WARNING(
        "JSON OCLA Debug Subsystem has extra object key. Expected %d count, "
        "found %d",
        1 + params.size() + str_params.size(), ocla_debug_subsystem.size());
    goto VALIDATE_OCLA_DEBUG_SUBSYSTEM_END;
  }
  status = true;
VALIDATE_OCLA_DEBUG_SUBSYSTEM_END:
  return status;
}
