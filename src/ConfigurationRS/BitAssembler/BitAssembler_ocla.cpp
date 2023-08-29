#include "BitAssembler_ocla.h"

BitAssembler_OCLA_REPORT::BitAssembler_OCLA_REPORT(std::string filepath) {
  stdout.open(filepath.c_str());
  CFG_ASSERT_MSG(stdout.is_open(), "Fail to open %s for writing",
                 filepath.c_str());
}

void BitAssembler_OCLA_REPORT::write(std::string msg) {
  CFG_ASSERT(stdout.is_open());
  stdout.write(msg.c_str(), msg.size());
  stdout.flush();
}

void BitAssembler_OCLA_REPORT::close() { stdout.close(); }

#define write_report(...) \
  { report.write(CFG_print(__VA_ARGS__)); }

struct BitAssembler_OCLA_PROBE {
  BitAssembler_OCLA_PROBE(std::string n) : name(n) {
    CFG_get_rid_whitespace(name);
    status = name.size() > 0;
    if (status) {
      size_t index = name.rfind("[");
      if (index != std::string::npos && name[name.size() - 1] == ']') {
        std::string range_string =
            name.substr(index + 1, name.size() - index - 2);
        std::vector<std::string> words = CFG_split_string(range_string, ":");
        if (words.size() == 2) {
          int w0 = (int)(CFG_convert_string_to_u64(words[0], true, &status));
          if (status) {
            int w1 = (int)(CFG_convert_string_to_u64(words[1], true, &status));
            if (status) {
              if (w1 == w0) {
                size = 1;
              } else if (w1 > w0) {
                size = (uint32_t)(w1 - w0) + 1;
              } else {
                size = (uint32_t)(w0 - w1) + 1;
              }
            } else {
              error = "Invalid number conversion";
            }
          } else {
            error = "Invalid number conversion";
          }
        } else if (words.size() == 1) {
          CFG_convert_string_to_u64(words[0], true, &status);
          if (status) {
            size = 1;
          } else {
            error = "Invalid number conversion";
          }
        } else {
          error = "Invalid splitter \":\" for bus range";
          status = false;
        }
      }
    } else {
      error = "Empty name";
    }
  }
  bool status = true;
  uint32_t size = 0;
  std::string name = "";
  std::string error = "";
};

struct BitAssembler_OCLA_INSTANTIATION {
  BitAssembler_OCLA_INSTANTIATION(std::string in, std::string im, std::string n,
                                  std::string f, std::string l, uint32_t no,
                                  bool t)
      : instantiator_name(in),
        instantiator_module(im),
        instance_name(n),
        filepath(f),
        language(l),
        line(no),
        is_top_module(t) {}
  std::string instantiator_name = "";
  std::string instantiator_module = "";
  std::string instance_name = "";
  std::string filepath = "";
  std::string language = "";
  uint32_t line = 0;
  bool is_top_module = false;
};

struct BitAssembler_IP_MODULE {
  BitAssembler_IP_MODULE() {
    value_maps["IP_TYPE"] = &type;
    value_maps["IP_VERSION"] = &version;
    value_maps["IP_ID"] = &id;
  }
  ~BitAssembler_IP_MODULE() {
    while (instantiations.size()) {
      CFG_MEM_DELETE(instantiations.back());
      instantiations.pop_back();
    }
  }
  void update_param(std::string param, BitAssembler_OCLA_REPORT& report) {
    write_report("      Param: %s\n", param.c_str());
    std::vector<std::string> words = CFG_split_string(param, "=");
    if (words.size() == 2) {
      if (value_maps.find(words[0]) != value_maps.end()) {
        write_report("        Supported\n");
        uint32_t* value_ptr = value_maps[words[0]];
        std::vector<std::string> values = CFG_split_string(words[1], "'");
        bool status = true;
        uint64_t temp = 0;
        std::string vstr = "";
        if (values.size() == 1) {
          vstr = values[0];
        } else {
          vstr = values[1];
        }
        temp = CFG_convert_string_to_u64(vstr, true, &status);
        if (status) {
          backup_values[words[0]] = *value_ptr;
          *value_ptr = (uint32_t)(temp);
          write_report("        Overwrite to 0x%08X\n", temp);
        } else {
          write_report("        Invalid number conversion\n");
          CFG_POST_WARNING("Invalid number conversion - %s", param.c_str());
        }
      }
    }
  }
  void backup_param() {
    for (auto& param : backup_values) {
      *value_maps[param.first] = param.second;
    }
    backup_values.clear();
  }
  std::string module_name = "";
  std::string module_secondary_name = "";
  uint32_t type = 0;
  uint32_t version = 0;
  uint32_t id = 0;
  std::map<std::string, uint32_t*> value_maps;
  std::map<std::string, uint32_t> backup_values;
  std::vector<BitAssembler_OCLA_INSTANTIATION*> instantiations;
};

struct BitAssembler_OCLA_MODULE : BitAssembler_IP_MODULE {
  BitAssembler_OCLA_MODULE() : BitAssembler_IP_MODULE() {
    value_maps["AXI_ADDR_WIDTH"] = &axi_addr_width;
    value_maps["AXI_DATA_WIDTH"] = &axi_data_width;
    value_maps["MEM_DEPTH"] = &mem_depth;
    value_maps["NO_OF_PROBES"] = &probes_count;
    value_maps["NO_OF_TRIGGER_INPUTS"] = &trigger_inputs_count;
  }
  uint32_t axi_addr_width = 0;
  uint32_t axi_data_width = 0;
  uint32_t mem_depth = 0;
  uint32_t probes_count = 0;
  uint32_t trigger_inputs_count = 0;
};

struct BitAssembler_AXIL_MODULE : BitAssembler_IP_MODULE {
  BitAssembler_AXIL_MODULE() : BitAssembler_IP_MODULE() {
    value_maps["ADDR_WIDTH"] = &addr_width;
    value_maps["DATA_WIDTH"] = &data_width;
    value_maps["M_COUNT"] = &count;
    for (int i = 0; i < 16; i++) {
      value_maps[CFG_print("M%02d_BASE_ADDR", i)] = &base_addresses[i];
    }
  }
  uint32_t addr_width = 0;
  uint32_t data_width = 0;
  uint32_t count = 0;
  uint32_t base_addresses[16];
};

struct BitAssembler_OCLA_INSTANCE {
  ~BitAssembler_OCLA_INSTANCE() {
    while (probes.size()) {
      CFG_MEM_DELETE(probes.back());
      probes.pop_back();
    }
    while (trigger_inputs.size()) {
      CFG_MEM_DELETE(trigger_inputs.back());
      trigger_inputs.pop_back();
    }
  }
  void update_param(BitAssembler_OCLA_MODULE*& ocla,
                    BitAssembler_OCLA_REPORT& report) {
    size_t index = module_unique_name.rfind("(");
    if (index > 0 && index != std::string::npos &&
        module_unique_name[module_unique_name.size() - 1] == ')') {
      std::string param_string = module_unique_name.substr(
          index + 1, module_unique_name.size() - index - 2);
      write_report("    Found Potential Param to overwrite: %s\n",
                   param_string.c_str());
      std::vector<std::string> params = CFG_split_string(param_string, ",");
      for (std::string& param : params) {
        ocla->update_param(param, report);
      }
    }
  }
  std::string module = "";
  std::string module_unique_name = "";
  std::string name = "";
  std::string instantiator_module = "";
  std::string instantiator_name = "";
  std::string instantiator_filepath = "";
  std::string instantiator_language = "";
  uint32_t instantiator_line = 0;
  bool instantiator_is_top_module = false;
  uint32_t axi_addr_width = 0;
  uint32_t axi_data_width = 0;
  uint32_t id = 0;
  uint32_t version = 0;
  uint32_t type = 0;
  uint32_t mem_depth = 0;
  uint32_t probes_count = 0;
  uint32_t trigger_inputs_count = 0;
  std::vector<BitAssembler_OCLA_PROBE*> probes;
  std::vector<BitAssembler_OCLA_PROBE*> trigger_inputs;
  std::string awready = "";
  uint32_t axil_connection = 0;
  uint32_t base_addr = 0;
};

void BitAssembler_OCLA::parse(CFGObject_BITOBJ& bitobj,
                              const std::string& taskPath,
                              const std::string& yosysBin,
                              const std::string& hierPath,
                              const std::string& ysPath) {
  if (std::filesystem::exists(hierPath) && std::filesystem::exists(ysPath)) {
    CFG_POST_MSG("  OCLA Parser");
    CFG_POST_MSG("    Design hierarchy: %s", hierPath.c_str());
    std::ifstream file(hierPath.c_str());
    CFG_ASSERT(file.is_open() && file.good());
    nlohmann::json hier = nlohmann::json::parse(file);
    file.close();
    std::string rptPath = CFG_print("%s/ocla.rpt", taskPath.c_str());
    BitAssembler_OCLA_REPORT report(rptPath);

    std::vector<BitAssembler_OCLA_MODULE*> ocla_modules;
    std::vector<BitAssembler_AXIL_MODULE*> axil_modules;
    get_ip_wrapper_module(hier, "ocla", 0x6F636C61, ocla_modules, report);
    if (ocla_modules.size()) {
      get_ip_wrapper_module(hier, "axil_interconnect", 0x6178696C, axil_modules,
                            report);
    }
    bool status = check_wrapper_modules(ocla_modules, axil_modules);
    if (status) {
      std::string rtlilPath = CFG_print("%s/ocla.full.rtlil", taskPath.c_str());
      status = generate_full_rtlil(yosysBin, ysPath, taskPath, rtlilPath);
      std::vector<std::pair<std::string, std::string>> cells;
      if (status) {
        std::vector<std::string> modules;
        for (BitAssembler_OCLA_MODULE*& ocla : ocla_modules) {
          modules.push_back(ocla->module_name);
        }
        status = get_modules(modules, cells, rtlilPath, report);
      }
      rtlilPath = CFG_print("%s/ocla.rtlil", taskPath.c_str());
      if (status) {
        status = generate_rtlil(cells, yosysBin, ysPath, taskPath, rtlilPath);
      }
      if (status) {
        std::vector<std::string> modules;
        std::vector<BitAssembler_OCLA_INSTANCE*> instances;
        for (auto& cell : cells) {
          modules.push_back(cell.second);
        }
        uint32_t connection_start_line = 0;
        for (auto& cell : cells) {
          status =
              status && get_probes(modules, instances, rtlilPath, cell.first,
                                   cell.second, connection_start_line, report);
          if (!status) {
            CFG_POST_WARNING(
                "Fail to retrieve OCLA probes information. Not able to "
                "auto-determine OCLA info");
            break;
          }
        }
        if (status) {
          write_report("Match RTLIL detected IP with hierarchy detected IP\n");
          for (BitAssembler_OCLA_INSTANCE*& instance : instances) {
            status = status && match_instance(instance, ocla_modules, report);
            if (!status) {
              CFG_POST_WARNING(
                  "Fail to match OCLA instance. Not able to auto-determine "
                  "OCLA info");
              break;
            }
          }
        }
        if (status) {
          write_report("Find AXIL connection (Start line: %d)\n",
                       connection_start_line);
          std::vector<std::string> axil_connections;
          std::vector<uint32_t> found_axil_connections;
          for (uint32_t i = 0; i < axil_modules[0]->count; i++) {
            axil_connections.push_back(CFG_print(
                "%s.m%02d_axil_awready",
                axil_modules[0]->instantiations[0]->instance_name.c_str(), i));
          }
          for (BitAssembler_OCLA_INSTANCE*& instance : instances) {
            std::string awready = instance->awready;
            size_t index = awready.rfind(" [");
            if (index != std::string::npos) {
              awready.erase(index, 1);
            }
            status = status &&
                     get_connection(rtlilPath, instance, axil_modules[0],
                                    axil_connections, found_axil_connections,
                                    awready, connection_start_line, 0, report);
            if (!status) {
              CFG_POST_WARNING(
                  "Fail to get OCLA to AXIL connection. Not able to "
                  "auto-determine OCLA info");
              break;
            }
          }
        }
        if (status) {
          CFG_POST_MSG("  Final OCLA info:");
          for (BitAssembler_OCLA_INSTANCE*& instance : instances) {
            CFG_POST_MSG(
                "    Instance: %s (module: %s, ID: 0x%08X) is connected to "
                "AXIL m%02d with "
                "base address 0x%08X",
                instance->name.c_str(), instance->module.c_str(), instance->id,
                instance->axil_connection, instance->base_addr);
            std::reverse(instance->probes.begin(), instance->probes.end());
            for (BitAssembler_OCLA_PROBE*& signal : instance->probes) {
              CFG_POST_MSG("      Probe: %s (size=%d)", signal->name.c_str(),
                           signal->size);
            }
            std::reverse(instance->trigger_inputs.begin(),
                         instance->trigger_inputs.end());
            for (BitAssembler_OCLA_PROBE*& signal : instance->trigger_inputs) {
              CFG_POST_MSG("      Trigger Input: %s (size=%d)",
                           signal->name.c_str(), signal->size);
            }
          }
        }
        while (instances.size()) {
          CFG_MEM_DELETE(instances.back());
          instances.pop_back();
        }
      }
    }
    while (ocla_modules.size()) {
      CFG_MEM_DELETE(ocla_modules.back());
      ocla_modules.pop_back();
    }
    while (axil_modules.size()) {
      CFG_MEM_DELETE(axil_modules.back());
      axil_modules.pop_back();
    }
    report.close();
  }
}

template <typename T>
void BitAssembler_OCLA::get_ip_wrapper_module(
    nlohmann::json& hier, const std::string& ip_name, const uint32_t ip_type,
    std::vector<T*>& ip_modules, BitAssembler_OCLA_REPORT& report) {
  std::vector<T*> temp_modules;
  if (hier.is_object() && hier.contains("modules") &&
      hier["modules"].is_object()) {
    write_report(
        "Reading hierarchy info to retrive module wrapper %s (Type: 0x%08X)\n",
        ip_name.c_str(), ip_type);
    nlohmann::json& modules = hier["modules"];
    for (auto& module : modules.items()) {
      nlohmann::json key = module.key();
      if (key.is_string() && modules[std::string(key)].is_object()) {
        nlohmann::json& module_wrapper = modules[std::string(key)];
        if (module_wrapper.contains("module") &&
            module_wrapper["module"].is_string() &&
            module_wrapper.contains("moduleInsts") &&
            module_wrapper["moduleInsts"].is_array() &&
            module_wrapper["moduleInsts"].size() == 1) {
          std::string module_name = std::string(module_wrapper["module"]);
          nlohmann::json& ocla = module_wrapper["moduleInsts"][0];
          if (ocla.is_object() && ocla.contains("instName") &&
              ocla["instName"].is_string() &&
              std::string(ocla["instName"]) == ip_name &&
              ocla.contains("parameters") && ocla["parameters"].is_array()) {
            write_report("  Detected the module\n");
            nlohmann::json& parameters = ocla["parameters"];
            bool status = true;
            std::vector<std::string> tracked_params;
            T* ip_module = CFG_MEM_NEW(T);
            ip_module->module_name = module_name;
            ip_module->module_secondary_name = std::string(key);
            for (nlohmann::json& parameter : parameters) {
              if (parameter.is_object() && parameter.contains("name") &&
                  parameter["name"].is_string() &&
                  parameter.contains("value") &&
                  parameter["value"].is_string()) {
                std::string name = std::string(parameter["name"]);
                if (ip_module->value_maps.find(name) ==
                    ip_module->value_maps.end()) {
                  continue;
                }
                uint32_t value = uint32_t(CFG_convert_string_to_u64(
                    std::string(parameter["value"]), true, &status));
                if (!status) {
                  write_report("  Invalid number conversion\n");
                  break;
                }
                if (CFG_find_string_in_vector(tracked_params, name) >= 0) {
                  write_report("  Duplicated parameter %d definition\n",
                               name.c_str());
                  status = false;
                  break;
                }
                tracked_params.push_back(name);
                *(ip_module->value_maps[name]) = value;
              }
            }
            if (status) {
              if (ip_module->value_maps.size() == tracked_params.size()) {
                if (ip_module->type == ip_type) {
                  temp_modules.push_back(ip_module);
                } else {
                  write_report("  Mismatch IP type. Detected 0x%08X\n",
                               ip_module->type);
                  status = false;
                }
              } else {
                write_report("  Not able to retrieve all expected parameter\n");
                status = false;
              }
            }
            if (!status) {
              CFG_MEM_DELETE(ip_module);
            }
          }
        }
      }
    }
  }
  bool status = true;
  for (T*& ip_module : temp_modules) {
    BitAssembler_IP_MODULE* ip =
        static_cast<BitAssembler_IP_MODULE*>(ip_module);
    write_report("  Get instantiator for %s (secondary name: %s)\n",
                 ip->module_name.c_str(), ip->module_secondary_name.c_str());
    status = get_module_instantiations(ip, hier);
    if (status) {
      if (ip->instantiations.size()) {
        ip_modules.push_back(ip_module);
      } else {
        write_report("    Not able to detect any instantiator\n");
      }
    } else {
      break;
    }
  }
  if (!status) {
    while (temp_modules.size()) {
      CFG_MEM_DELETE(temp_modules.back());
      temp_modules.pop_back();
    }
    ip_modules.clear();
  }
  for (auto& ip : temp_modules) {
    if (std::find(ip_modules.begin(), ip_modules.end(), ip) ==
        ip_modules.end()) {
      CFG_MEM_DELETE(ip);
    }
  }
}

bool BitAssembler_OCLA::get_module_instantiations(BitAssembler_IP_MODULE*& ip,
                                                  nlohmann::json& hier) {
  bool status = false;
  if (hier.is_object() && hier.contains("fileIDs") &&
      hier["fileIDs"].is_object()) {
    // Top
    if (hier.contains("hierTree") && hier["hierTree"].is_array() &&
        hier["hierTree"].size() == 1 && hier["hierTree"][0].is_object()) {
      nlohmann::json& top = hier["hierTree"][0];
      status = get_module_instantiations(ip, top, true, "top_level",
                                         "topModule", hier["fileIDs"]);
    } else {
      CFG_POST_WARNING("Could not detect top level module - hierTree");
    }
    // Modules
    if (status && hier.contains("modules") && hier["modules"].is_object()) {
      nlohmann::json& modules = hier["modules"];
      for (auto& module : modules.items()) {
        nlohmann::json key = module.key();
        if (key.is_string() && modules[std::string(key)].is_object()) {
          status = get_module_instantiations(ip, modules[std::string(key)],
                                             false, std::string(key), "module",
                                             hier["fileIDs"]);
        } else {
          CFG_POST_WARNING("Could not detect module instance name");
          status = false;
        }
        if (!status) {
          break;
        }
      }
    }
  } else {
    CFG_POST_WARNING("Could not detect file information - fileIDs");
  }
  return status;
}

bool BitAssembler_OCLA::get_module_instantiations(
    BitAssembler_IP_MODULE*& ip, nlohmann::json& module, bool top,
    std::string module_name, std::string module_key, nlohmann::json& fileIDs) {
  bool status = false;
  if (module.contains("file") && module["file"].is_string() &&
      module.contains("language") && module["language"].is_string() &&
      module.contains(module_key) && module[module_key].is_string()) {
    if (module.contains("moduleInsts") && module["moduleInsts"].is_array()) {
      std::string instantiator_module = std::string(module[module_key]);
      std::string instantiator_name = top ? instantiator_module : module_name;
      std::string language = std::string(module["language"]);
      status = get_module_instantiations(ip, language, instantiator_name,
                                         instantiator_module, top,
                                         module["moduleInsts"], fileIDs);
      if (!status) {
        CFG_POST_WARNING("Could not detect %s module moduleInsts",
                         module_name.c_str());
      }
    } else {
      status = true;
    }
  } else {
    CFG_POST_WARNING("Could not detect %s module info", module_name.c_str());
  }
  return status;
}

bool BitAssembler_OCLA::get_module_instantiations(
    BitAssembler_IP_MODULE*& ip, std::string language,
    std::string instantiator_name, std::string instantiator_module, bool top,
    nlohmann::json& moduleInsts, nlohmann::json& fileIDs) {
  bool status = true;
  for (nlohmann::json& moduleInst : moduleInsts) {
    if (moduleInst.is_object()) {
      if (!moduleInst.contains("file")) {
        CFG_POST_WARNING("Missing \"file\" for module in moduleInsts");
        status = false;
      }
      if (!moduleInst.contains("instName")) {
        CFG_POST_WARNING("Missing \"instName\" for module in moduleInsts");
        status = false;
      }
      if (!moduleInst.contains("line")) {
        CFG_POST_WARNING("Missing \"line\" for module in moduleInsts");
        status = false;
      }
      if (!moduleInst.contains("module")) {
        CFG_POST_WARNING("Missing \"module\" for module in moduleInsts");
        status = false;
      }
      if (status && !(moduleInst["file"].is_string() &&
                      moduleInst["instName"].is_string() &&
                      (moduleInst["line"].is_number_integer() ||
                       moduleInst["line"].is_number_unsigned()) &&
                      moduleInst["module"].is_string())) {
        CFG_POST_WARNING("Module in moduleInsts has unexpected type");
        status = false;
      }
      if (status && !(fileIDs.contains(std::string(moduleInst["file"])) &&
                      fileIDs[std::string(moduleInst["file"])].is_string())) {
        CFG_POST_WARNING("Could not match file with ID %s",
                         std::string(moduleInst["file"]).c_str());
        status = false;
      }
      if (status) {
        std::string file_id = std::string(moduleInst["file"]);
        std::string file = std::string(fileIDs[file_id]);
        std::string instName = std::string(moduleInst["instName"]);
        std::string module = std::string(moduleInst["module"]);
        uint32_t line = (uint32_t)(moduleInst["line"]);
#if 0
        printf("Top: %s\n", instantiator_name.c_str());
        printf("  Module: %s\n", module.c_str());
#endif
        if (module == ip->module_name) {
          ip->instantiations.push_back(CFG_MEM_NEW(
              BitAssembler_OCLA_INSTANTIATION, instantiator_name,
              instantiator_module, instName, file, language, line, top));
        } else if (module == ip->module_secondary_name) {
          ip->instantiations.push_back(CFG_MEM_NEW(
              BitAssembler_OCLA_INSTANTIATION, instantiator_name,
              instantiator_module, instName, file, language, line, top));
        }
      } else {
        break;
      }
    } else {
      CFG_POST_WARNING("Module in moduleInsts is not an object");
      status = false;
      break;
    }
  }
  return status;
}

bool BitAssembler_OCLA::check_wrapper_modules(
    std::vector<BitAssembler_OCLA_MODULE*>& oclas,
    std::vector<BitAssembler_AXIL_MODULE*>& axils) {
  bool status = false;
  if (oclas.size()) {
    if (axils.size()) {
      if (axils.size() == 1 && axils[0]->instantiations.size() == 1 &&
          axils[0]->count > 0 && axils[0]->count <= 16) {
        status = true;
        CFG_POST_MSG("  Detected OCLA module wrapper");
        for (auto& ip : oclas) {
          CFG_POST_MSG("    Module name: %s", ip->module_name.c_str());
          CFG_POST_MSG("      Probe(s) count: %d", ip->probes_count);
          CFG_POST_MSG("      Trigger Input count: %d",
                       ip->trigger_inputs_count);
          for (BitAssembler_OCLA_INSTANTIATION*& inst : ip->instantiations) {
            CFG_POST_MSG("      Instantiated by %s (as %s)",
                         inst->instantiator_module.c_str(),
                         inst->instance_name.c_str());
            CFG_POST_MSG("        @File %s (Line %d)", inst->filepath.c_str(),
                         inst->line);
          }
        }
        CFG_POST_MSG("  Detected AXIL module wrapper");
        for (auto& ip : axils) {
          CFG_POST_MSG("    Module name: %s", ip->module_name.c_str());
          CFG_POST_MSG("      Count: %d", ip->count);
          CFG_POST_MSG("      Base Address(s):");
          for (uint32_t i = 0; i < ip->count; i++) {
            CFG_POST_MSG("        %s: 0x%08X",
                         CFG_print("M%02d_BASE_ADDR", i).c_str(),
                         ip->base_addresses[i]);
          }
          for (BitAssembler_OCLA_INSTANTIATION*& inst : ip->instantiations) {
            CFG_POST_MSG("      Instantiated by %s (as %s)",
                         inst->instantiator_module.c_str(),
                         inst->instance_name.c_str());
            CFG_POST_MSG("        @File %s (Line %d)", inst->filepath.c_str(),
                         inst->line);
          }
        }
      } else {
        CFG_POST_WARNING(
            "  Detected multiple AXIL module wrapper (count=%d) or it is "
            "instantiated multiple times (count=%d) or invalid M_COUNT=%d",
            axils.size(), axils[0]->instantiations.size(), axils[0]->count);
      }
    } else {
      CFG_POST_WARNING("  Do not detect any AXIL wrapper module");
    }
  } else {
    CFG_POST_WARNING("  Do not detect any OCLA wrapper module");
  }
  return status;
}

bool BitAssembler_OCLA::generate_full_rtlil(const std::string& yosysBin,
                                            const std::string& ysPath,
                                            const std::string& taskPath,
                                            const std::string& rtlilPath) {
  // Read text line by line
  std::ifstream file(ysPath.c_str());
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", ysPath.c_str());
  std::string outPath = CFG_print("%s/ocla.full.ys", taskPath.c_str());
  std::ofstream outfile(outPath.c_str());
  CFG_ASSERT_MSG(outfile.is_open(), "Fail to open %s for writing",
                 outPath.c_str());
  std::string line = "";
  std::string temp = "\n";
  while (getline(file, line)) {
    outfile << line.c_str() << "\n";
    if (line.find("hierarchy -top ") == 0) {
      break;
    }
  }
  outfile << "\n# Start OCLA RTLIL flow\n";
  outfile << CFG_print("write_rtlil %s\n", rtlilPath.c_str()).c_str();
  file.close();
  outfile.close();
  temp = CFG_print("%s %s", yosysBin.c_str(), outPath.c_str());
  std::string cmd_output = "";
  std::atomic<bool> stop = false;
  return CFG_execute_cmd(temp, cmd_output, nullptr, stop) == 0;
}

bool BitAssembler_OCLA::generate_rtlil(
    const std::vector<std::pair<std::string, std::string>>& cells,
    const std::string& yosysBin, const std::string& ysPath,
    const std::string& taskPath, const std::string& rtlilPath) {
  // Read text line by line
  std::ifstream file(ysPath.c_str());
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", ysPath.c_str());
  std::string outPath = CFG_print("%s/ocla.ys", taskPath.c_str());
  std::ofstream outfile(outPath.c_str());
  CFG_ASSERT_MSG(outfile.is_open(), "Fail to open %s for writing",
                 outPath.c_str());
  std::string line = "";
  std::string temp = "\n";
  while (getline(file, line)) {
    outfile << line.c_str() << "\n";
    if (line.find("hierarchy -top ") == 0) {
      break;
    }
  }
  outfile << "\n# Start OCLA RTLIL flow\n";
  for (auto& cell : cells) {
    outfile << CFG_print("blackbox %s\n", cell.second.c_str()).c_str();
  }
  outfile << "flatten\n";
  outfile << CFG_print("write_rtlil %s\n", rtlilPath.c_str()).c_str();
  file.close();
  outfile.close();
  temp = CFG_print("%s %s", yosysBin.c_str(), outPath.c_str());
  std::string cmd_output = "";
  std::atomic<bool> stop = false;
  return CFG_execute_cmd(temp, cmd_output, nullptr, stop) == 0;
}

bool BitAssembler_OCLA::get_modules(
    const std::vector<std::string>& modules,
    std::vector<std::pair<std::string, std::string>>& cells,
    const std::string& rtlilPath, BitAssembler_OCLA_REPORT& report) {
  // Read text line by line
  std::ifstream file(rtlilPath.c_str());
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", rtlilPath.c_str());
  write_report("Parse FULL RTLIL to get final module:\n");
  write_report("  %s", CFG_print_strings_to_string(modules, ",").c_str());
  std::string line = "";
  size_t line_no = 0;
  while (getline(file, line)) {
    line_no++;
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0 || line[0] == '#') {
      // allow blank line
      continue;
    }
    if (line.find("module \\") == 0) {
      std::vector<std::string> words = CFG_split_string(line.substr(8), " ");
      if (words.size()) {
        for (auto& module : modules) {
          if (words[0] == module ||
              (words[0].find(module) == 0 && words[0][module.size()] == '(')) {
            cells.push_back(std::make_pair(module, words[0]));
            break;
          }
        }
      }
    }
  }
  file.close();
  write_report("  Found total of %d module%s\n", cells.size(),
               cells.size() == 0 ? "" : "(s):");
  for (auto& cell : cells) {
    write_report("    %s -> %s\n", cell.first.c_str(), cell.second.c_str());
  }
  for (auto& module : modules) {
    bool found = false;
    for (auto& cell : cells) {
      if (module == cell.first) {
        found = true;
        break;
      }
    }
    if (!found) {
      CFG_POST_WARNING("Fail to find module %s", module.c_str());
      write_report("  Warning: Fail to find module %s\n", module.c_str());
    }
  }
  return cells.size() > 0;
}

bool BitAssembler_OCLA::get_probes(
    const std::vector<std::string>& modules,
    std::vector<BitAssembler_OCLA_INSTANCE*>& instances,
    const std::string& rtlilPath, const std::string& module,
    const std::string& module_instance, uint32_t& connection_start_line,
    BitAssembler_OCLA_REPORT& report) {
  bool status = true;
  // Read text line by line
  std::ifstream file(rtlilPath.c_str());
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", rtlilPath.c_str());
  write_report("Parse RTLIL for module %s (%s)\n", module.c_str(),
               module_instance.c_str());
  std::string line = "";
  bool found_instantiator = false;
  bool found_module = false;
  size_t instantiator_line = 0;
  size_t line_no = 0;
  while (getline(file, line)) {
    line_no++;
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0 || line[0] == '#') {
      // allow blank line
      continue;
    }
    if (line.find("module \\") == 0) {
      if (found_instantiator) {
        break;
      }
      std::vector<std::string> words = CFG_split_string(line.substr(8), " ");
      if (words.size()) {
        if (CFG_find_string_in_vector(modules, words[0]) == -1) {
          found_instantiator = true;
          instantiator_line = line_no;
          connection_start_line = line_no;
          write_report("  Line %d: Found instantiator %s\n", line_no,
                       words[0].c_str());
        }
      } else {
        status = false;
        write_report("  Line %d: Fail to get module name (%s)\n", line_no,
                     line.c_str());
        break;
      }
    } else if (found_instantiator && !found_module &&
               line.find("  cell \\") == 0) {
      std::vector<std::string> words = CFG_split_string(line.substr(8), " ");
      if (words.size()) {
        if (words[0] == module_instance) {
          instances.push_back(CFG_MEM_NEW(BitAssembler_OCLA_INSTANCE));
          instances.back()->module = module;
          instances.back()->module_unique_name = module_instance;
          instances.back()->name = module;
          found_module = true;
          if (words.size() >= 2 && words[1].size() > 0 && words[1][0] == '\\') {
            instances.back()->name = words[1].substr(1);
          }
          write_report(
              "    Line %d: Found targeted cell (module: %s, "
              "instance: %s)\n",
              line_no, instances.back()->module.c_str(),
              instances.back()->name.c_str());
        }
      } else {
        status = false;
        write_report("  Line %d: Fail to get cell name (%s)\n", line_no,
                     line.c_str());
        break;
      }
    } else if (found_instantiator && found_module && line == "  end") {
      found_module = false;
      connection_start_line = line_no;
    } else if (found_instantiator && !found_module &&
               line.find("  connect ") == 0) {
      connection_start_line = line_no;
      break;
    } else if (found_instantiator && found_module &&
               (line.find("    connect \\i_probes ") == 0 ||
                line.find("    connect \\i_trigger_input ") == 0)) {
      std::vector<BitAssembler_OCLA_PROBE*>* signals = nullptr;
      if (line.find("    connect \\i_probes ") == 0) {
        line = line.substr(22);
        signals = &instances.back()->probes;
        write_report("      Line %d: Probe(s):\n", line_no);
      } else {
        line = line.substr(29);
        signals = &instances.back()->trigger_inputs;
        write_report("      Line %d: Trigger Input(s):\n", line_no);
      }
      write_report("        %s\n", line.c_str());
      if (signals->size()) {
        status = false;
        write_report("  Line %d: Duplicated signal(s)\n", line_no);
        break;
      }
      CFG_get_rid_whitespace(line);
      if (line.size() > 4 && line.find("{ ") == 0 &&
          line.find(" }") == (line.size() - 2)) {
        line = line.substr(2, line.size() - 4);
        size_t index = line.find(" ");
        while (index != std::string::npos) {
          if ((index + 1) < line.size()) {
            if (line[index + 1] == '[') {
              index = line.find(" ", index + 1);
              continue;
            }
          }
          signals->push_back(
              CFG_MEM_NEW(BitAssembler_OCLA_PROBE, line.substr(0, index)));
          line = line.substr(index + 1);
          CFG_get_rid_whitespace(line);
          index = line.find(" ");
          if (!signals->back()->status) {
            status = false;
            write_report("  Line %d: %s\n", line_no,
                         signals->back()->error.c_str());
            break;
          }
        }
        CFG_get_rid_whitespace(line);
        if (line.size()) {
          signals->push_back(CFG_MEM_NEW(BitAssembler_OCLA_PROBE, line));
          if (!signals->back()->status) {
            status = false;
            write_report("  Line %d: %s\n", line_no,
                         signals->back()->error.c_str());
            break;
          }
        }
      } else {
        signals->push_back(CFG_MEM_NEW(BitAssembler_OCLA_PROBE, line));
        if (!signals->back()->status) {
          status = false;
          write_report("  Line %d: %s\n", line_no,
                       signals->back()->error.c_str());
          break;
        }
      }
      if (signals->size()) {
        for (auto& signal : *signals) {
          write_report("          %s\n", signal->name.c_str());
          if (signal->name.size() == 0) {
            status = false;
            write_report("  Line %d: Blank/Invalid signal(s)\n", line_no);
            break;
          }
        }
      } else {
        status = false;
        write_report("  Line %d: Fail to get any signals\n", line_no);
        break;
      }
    } else if (found_instantiator && found_module &&
               line.find("    connect \\s_axil_awready ") == 0) {
      line = line.substr(28);
      CFG_get_rid_whitespace(line);
      write_report("      Line %d: awready:\n", line_no);
      write_report("        %s\n", line.c_str());
      if (instances.back()->awready.size()) {
        status = false;
        write_report("  Line %d: Duplicated awready(s)\n", line_no);
        break;
      }
      instances.back()->awready = line;
      if (line.size() > 4 && line.find("{ ") == 0 &&
          line.find(" }") == (line.size() - 2)) {
        status = false;
        write_report("  Line %d: Fail because awready should be a single bit\n",
                     line_no);
        break;
      }
      if (instances.back()->awready.size() == 0) {
        status = false;
        write_report("  Line %d: Blank/Invalid awready(s)\n", line_no);
        break;
      }
    }
    if (!status) {
      break;
    }
  }
  file.close();
  if (status) {
    for (auto& instance : instances) {
      if (instance->probes.size() == 0) {
        status = false;
        write_report("  Fail to get any probe for instance %s\n",
                     instance->name.c_str());
      }
      if (instance->awready.size() == 0) {
        status = false;
        write_report("  Fail to get s_axil_awready for instance %s\n",
                     instance->name.c_str());
      }
    }
  }
  if (status) {
    std::vector<std::string> signals;
    std::vector<std::vector<BitAssembler_OCLA_PROBE*>> signal_ptrs;
    for (auto& instance : instances) {
      for (auto& signal : instance->probes) {
        if (signal->size == 0) {
          int index = CFG_find_string_in_vector(signals, signal->name);
          if (index >= 0) {
            signal_ptrs[index].push_back(signal);
          } else {
            signals.push_back(signal->name);
            signal_ptrs.push_back({signal});
          }
        }
      }
      for (auto& signal : instance->trigger_inputs) {
        if (signal->size == 0) {
          int index = CFG_find_string_in_vector(signals, signal->name);
          if (index >= 0) {
            signal_ptrs[index].push_back(signal);
          } else {
            signals.push_back(signal->name);
            signal_ptrs.push_back({signal});
          }
        }
      }
    }
    write_report("  Number signals that need to figure the bus size: %d\n",
                 signals.size());
    for (auto& signal : signals) {
      write_report("    %s\n", signal.c_str());
    }
    status = get_probe_size(instantiator_line, rtlilPath, signals, signal_ptrs,
                            report);
    if (status) {
      write_report("  Final Signal Size Info:\n");
      for (auto& instance : instances) {
        write_report("    Instance: %s\n", instance->name.c_str());
        write_report("      Probe(s):\n");
        for (auto& signal : instance->probes) {
          write_report("        %s (size=%d)\n", signal->name.c_str(),
                       signal->size);
        }
        if (instance->trigger_inputs.size()) {
          write_report("      Trigger Input(s):\n");
        }
        for (auto& signal : instance->trigger_inputs) {
          write_report("        %s (size=%d)\n", signal->name.c_str(),
                       signal->size);
        }
      }
    }
  }
  return status;
}

bool BitAssembler_OCLA::get_probe_size(
    const size_t start_line, const std::string& rtlilPath,
    std::vector<std::string>& signals,
    std::vector<std::vector<BitAssembler_OCLA_PROBE*>>& signal_ptrs,
    BitAssembler_OCLA_REPORT& report) {
  // Read text line by line
  std::ifstream file(rtlilPath.c_str());
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", rtlilPath.c_str());
  bool status = true;
  std::string line = "";
  size_t line_no = 0;
  CFG_ASSERT(signals.size() == signal_ptrs.size());
  while (getline(file, line)) {
    line_no++;
    if (line_no <= start_line) {
      continue;
    }
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0 || line[0] == '#') {
      // allow blank line
      continue;
    }
    if (line.find("  cell \\") == 0) {
      break;
    } else if (line == "end") {
      break;
    } else if (line.find("  wire ") == 0) {
      line = line.substr(7);
      CFG_get_rid_whitespace(line);
      std::vector<std::string> words = CFG_split_string(line, " ");
      if (words.size() == 0) {
        status = false;
        write_report("  Line %d: Empty/Invalid wire", line_no);
        break;
      }
      int index = CFG_find_string_in_vector(signals, words[words.size() - 1]);
      if (index >= 0) {
        write_report("  Line %d: Found wire %s\n", line_no,
                     signals[index].c_str());
        if (words.size() >= 3 && words[0] == "width") {
          uint32_t width =
              (uint32_t)(CFG_convert_string_to_u64(words[1], true, &status));
          if (status) {
            int offset = 0;
            if (words.size() >= 5 && words[2] == "offset") {
              offset =
                  (int)(CFG_convert_string_to_u64(words[3], true, &status));
            }
            if (status) {
              for (auto& signal : signal_ptrs[index]) {
                signal->size = width;
                signal->name = CFG_print("%s [%d:%d]", signal->name.c_str(),
                                         offset + int(width) - 1, offset);
              }
            }
          }
          if (!status) {
            write_report("  Line %d: Invalid number conversion: %s", line_no,
                         line.c_str());
            break;
          }
        } else {
          for (auto& signal : signal_ptrs[index]) {
            signal->size = 1;
          }
        }
        signals.erase(signals.begin() + index);
        signal_ptrs.erase(signal_ptrs.begin() + index);
        if (signals.size() == 0) {
          break;
        }
      }
    }
  }
  file.close();
  if (signals.size()) {
    status = false;
    for (auto& signal : signals) {
      write_report("  Fail to get bus size of %s\n", signal.c_str());
    }
  }
  return status;
}

bool BitAssembler_OCLA::match_instance(
    BitAssembler_OCLA_INSTANCE*& instance,
    std::vector<BitAssembler_OCLA_MODULE*>& oclas,
    BitAssembler_OCLA_REPORT& report) {
  bool status = false;
  for (auto& ocla : oclas) {
    if (ocla->module_name == instance->module) {
      for (auto& inst : ocla->instantiations) {
        size_t index = instance->name.rfind(inst->instance_name);
        if (inst->instance_name == instance->name ||
            (index != std::string::npos &&
             ((index + inst->instance_name.size()) == instance->name.size()) &&
             (instance->name[index - 1] == '.'))) {
          write_report("  Match IP module: %s, instance: %s\n",
                       instance->module.c_str(), instance->name.c_str());
          instance->instantiator_module = inst->instantiator_module;
          instance->instantiator_name = inst->instantiator_name;
          instance->instantiator_filepath = inst->filepath;
          instance->instantiator_language = inst->language;
          instance->instantiator_line = inst->line;
          instance->instantiator_is_top_module = inst->is_top_module;
          instance->update_param(ocla, report);
          instance->axi_addr_width = ocla->axi_addr_width;
          instance->axi_data_width = ocla->axi_data_width;
          instance->id = ocla->id;
          instance->version = ocla->version;
          instance->type = ocla->type;
          instance->mem_depth = ocla->mem_depth;
          instance->probes_count = ocla->probes_count;
          instance->trigger_inputs_count = ocla->trigger_inputs_count;
          ocla->backup_param();
          status = true;
          break;
        }
      }
      if (status) {
        break;
      }
    }
  }
  if (!status) {
    write_report("  Not able to match IP module: %s, instance: %s\n",
                 instance->module.c_str(), instance->name.c_str());
  }
  return status;
}

bool BitAssembler_OCLA::get_connection(
    const std::string& rtlilPath, BitAssembler_OCLA_INSTANCE*& instance,
    BitAssembler_AXIL_MODULE*& axil_instance,
    std::vector<std::string>& axil_connections,
    std::vector<uint32_t>& found_axil_connections, const std::string& source,
    uint32_t start_line, uint32_t level, BitAssembler_OCLA_REPORT& report) {
  bool status = level < 100;
  // Read text line by line
  std::ifstream file;
  if (status) {
    write_report("  Trace source: %s (level: %d)\n", source.c_str(), level);
    file.open(rtlilPath.c_str(), std::ios::in);
    CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", rtlilPath.c_str());
  } else {
    write_report("  Fail because too many searching: %s\n", source.c_str());
  }

  std::string dest = "";
  std::string line = "";
  size_t line_no = 0;
  while (status && getline(file, line)) {
    line_no++;
    if (line_no < start_line) {
      continue;
    }
    // Only trim the trailing whitespace
    CFG_get_rid_trailing_whitespace(line);
    if (line.size() == 0 || line[0] == '#') {
      // allow blank line
      continue;
    }
    if (line == "end") {
      break;
    } else if (line.find("  connect ") == 0) {
      line = line.substr(10);
      size_t index = line.find(" [");
      while (index != std::string::npos) {
        line.erase(index, 1);
        index = line.find(" [");
      }
      std::vector<std::string> words = CFG_split_string(line, " ");
      if (words.size() == 2 && words[1] == source) {
        dest = words[0];
        break;
      }
    }
  }
  if (status) {
    file.close();
    if (dest.size()) {
      write_report("    Line %d: Detected destination %s\n", line_no,
                   dest.c_str());
      size_t i = 0;
      for (auto& axil : axil_connections) {
        size_t index = dest.rfind(axil);
        if (index != std::string::npos && index > 0 &&
            (index + axil.size()) == dest.size() &&
            (dest[index - 1] == '.' || dest[index - 1] == '\\')) {
          write_report("      Final destination: %s\n", dest.c_str());
          instance->axil_connection = (uint32_t)(i);
          instance->base_addr = axil_instance->base_addresses[i];
          if (CFG_find_u32_in_vector(found_axil_connections,
                                     instance->axil_connection) >= 0) {
            write_report("  Duplicate axil connection at m%02d\n",
                         instance->axil_connection);
            status = false;
          }
          break;
        }
        i++;
      }
      if (status && i == axil_connections.size()) {
        status = get_connection(rtlilPath, instance, axil_instance,
                                axil_connections, found_axil_connections, dest,
                                start_line, level + 1, report);
      }
    } else {
      write_report("  Fail to find destination\n");
      status = false;
    }
  }
  return status;
}
