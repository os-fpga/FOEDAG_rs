/*
Copyright 2022 RapidSilicon
All rights reserved
*/

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <process.h>
#else
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#endif

#include <QDebug>
#include <chrono>
#include <filesystem>
#include <thread>

#include "Compiler/CompilerRS.h"
#include "Compiler/Constraints.h"
#include "MainWindow/Session.h"
#include "NewProject/ProjectManager/project_manager.h"

#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

using namespace FOEDAG;

const std::string QLYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}

# Technology mapping
hierarchy -top ${TOP_MODULE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -family ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${NO_DSP} ${NO_BRAM} ${FSM_ENCODING} -blif ${OUTPUT_BLIF} ${FAST} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}


write_verilog -noattr -nohex ${OUTPUT_VERILOG}
  )";

const std::string RapidSiliconYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}

# Technology mapping
hierarchy -top ${TOP_MODULE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${NO_DSP} ${NO_BRAM} ${FSM_ENCODING} ${FAST} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

# Clean and output blif
write_blif ${OUTPUT_BLIF}
write_verilog -noexpr -nodec -norename ${OUTPUT_VERILOG}
  )";

std::string CompilerRS::InitSynthesisScript() {
  switch (m_synthType) {
    case SynthesisType::Yosys:
      Message("Yosys Synthesis");
      return CompilerOpenFPGA::InitSynthesisScript();
    case SynthesisType::QL: {
      Message("QL Synthesis");
      if (m_yosysPluginLib.empty()) m_yosysPluginLib = "ql-qlf";
      if (m_yosysPlugin.empty()) m_yosysPlugin = "synth_ql";
      YosysScript(QLYosysScript);
      break;
    }
    case SynthesisType::RS: {
      Message("RS Synthesis");
      if (m_mapToTechnology.empty()) m_mapToTechnology = "genesis";
      if (m_yosysPluginLib.empty()) m_yosysPluginLib = "synth-rs";
      if (m_yosysPlugin.empty()) m_yosysPlugin = "synth_rs";
      YosysScript(RapidSiliconYosysScript);
      break;
    }
  }
  return m_yosysScript;
}

std::string CompilerRS::FinishSynthesisScript(const std::string& script) {
  std::string result = script;
  // Keeps for Synthesis, preserve nodes used in constraints
  std::string keeps;
  if (m_keepAllSignals) {
    keeps += "setattr -set keep 1 w:\\*\n";
  }
  for (auto keep : m_constraints->GetKeeps()) {
    keep = ReplaceAll(keep, "@", "[");
    keep = ReplaceAll(keep, "%", "]");
    (*m_out) << "Keep name: " << keep << "\n";
    keeps += "setattr -set keep 1 " + keep + "\n";
  }
  result = ReplaceAll(result, "${KEEP_NAMES}", keeps);
  std::string optimization;
  switch (m_synthOpt) {
    case SynthesisOpt::None:
      // None means none
      // Aram: DE optimization and mapping is mandatory for July release.
      optimization = "-de";
      break;
    case SynthesisOpt::Area:
      optimization = "-de -goal area";
      break;
    case SynthesisOpt::Delay:
      optimization = "-de -goal delay";
      break;
    case SynthesisOpt::Mixed:
      optimization = "-de -goal mixed";
      break;
    case SynthesisOpt::Clean:
      break;
  }
  std::string effort;
  switch (m_synthEffort) {
    case SynthesisEffort::None:
      break;
    case SynthesisEffort::High:
      effort = "-effort high";
      break;
    case SynthesisEffort::Low:
      effort = "-effort low";
      break;
    case SynthesisEffort::Medium:
      effort = "-effort medium";
      break;
  }
  std::string fsm_encoding;
  switch (m_synthFsm) {
    case SynthesisFsmEncoding::None:
      break;
    case SynthesisFsmEncoding::Binary:
      fsm_encoding = "-fsm_encoding binary";
      break;
    case SynthesisFsmEncoding::Onehot:
      fsm_encoding = "-fsm_encoding onehot";
      break;
  }
  std::string carry_inference;
  switch (m_synthCarry) {
    case SynthesisCarryInference::None:
      break;
    case SynthesisCarryInference::All:
      carry_inference = "-carry all";
      break;
    case SynthesisCarryInference::Auto:
      carry_inference = "-carry auto";
      break;
    case SynthesisCarryInference::NoCarry:
      carry_inference = "-carry no";
      break;
  }
  std::string no_dsp;
  if (m_synthNoDsp) {
    no_dsp = "-no_dsp";
  }
  std::string no_bram;
  if (m_synthNoBram) {
    no_bram = "-no_bram";
  }
  std::string fast_mode;
  if (m_synthFast) {
    fast_mode = "-fast";
  }
  std::string max_threads = "-de_max_threads " + std::to_string(m_maxThreads);
  std::string no_simplify;
  if (m_synthNoSimplify) {
    no_simplify = "-no_simplify";
  }
  std::string clke_strategy;
  switch (m_synthClke) {
    case SynthesisClkeStrategy::None:
      break;
    case SynthesisClkeStrategy::Early:
      clke_strategy = "-clock_enable_strategy early";
      break;
    case SynthesisClkeStrategy::Late:
      clke_strategy = "-clock_enable_strategy late";
      break;
  }
  std::string cec;
  if (m_synthCec) {
    cec = "-cec";
  }

  if (m_synthType == SynthesisType::QL) {
    optimization = "";
    effort = "";
    fsm_encoding = "";
    fast_mode = "";
    carry_inference = "";
    if (m_synthNoAdder) {
      optimization += " -no_adder";
    }
    max_threads = "";
    no_simplify = "";
    clke_strategy = "";
    cec = "";
  }
  optimization += " " + PerDeviceSynthOptions();
  optimization += " " + SynthMoreOpt();
  result = ReplaceAll(result, "${OPTIMIZATION}", optimization);
  result = ReplaceAll(result, "${EFFORT}", effort);
  result = ReplaceAll(result, "${FSM_ENCODING}", fsm_encoding);
  result = ReplaceAll(result, "${CARRY}", carry_inference);
  result = ReplaceAll(result, "${NO_DSP}", no_dsp);
  result = ReplaceAll(result, "${NO_BRAM}", no_bram);
  result = ReplaceAll(result, "${FAST}", fast_mode);
  result = ReplaceAll(result, "${MAX_THREADS}", max_threads);
  result = ReplaceAll(result, "${NO_SIMPLIFY}", no_simplify);
  result = ReplaceAll(result, "${CLKE_STRATEGY}", clke_strategy);
  result = ReplaceAll(result, "${CEC}", cec);
  result = ReplaceAll(result, "${PLUGIN_LIB}", YosysPluginLibName());
  result = ReplaceAll(result, "${PLUGIN_NAME}", YosysPluginName());
  result = ReplaceAll(result, "${MAP_TO_TECHNOLOGY}", YosysMapTechnology());
  result = ReplaceAll(result, "${LUT_SIZE}", std::to_string(m_lut_size));
  return result;
}

CompilerRS::CompilerRS() : CompilerOpenFPGA() {
  m_synthType = SynthesisType::RS;
  m_channel_width = 180;
}

CompilerRS::~CompilerRS() {
#ifdef PRODUCTION_BUILD
  if (licensePtr) delete licensePtr;
#endif
}

bool CompilerRS::RegisterCommands(TclInterpreter* interp, bool batchMode) {
  CompilerOpenFPGA::RegisterCommands(interp, batchMode);

  auto synth_options = [](void* clientData, Tcl_Interp* interp, int argc,
                          const char* argv[]) -> int {
    CompilerRS* compiler = (CompilerRS*)clientData;
    if (compiler->m_synthType == SynthesisType::Yosys) {
      compiler->ErrorMessage("Please set 'synthesis_type RS or QL' first.");
      return TCL_ERROR;
    }
    for (int i = 1; i < argc; i++) {
      std::string option = argv[i];
      if (option == "-fsm_encoding" && i + 1 < argc) {
        std::string arg = argv[++i];
        if (arg == "binary") {
          compiler->SynthFsm(SynthesisFsmEncoding::Binary);
        } else if (arg == "onehot") {
          compiler->SynthFsm(SynthesisFsmEncoding::Onehot);
        } else {
          compiler->ErrorMessage("Unknown fsm encoding option: " + arg);
          return TCL_ERROR;
        }
        continue;
      }
      if (option == "-effort" && i + 1 < argc) {
        std::string arg = argv[++i];
        if (arg == "high") {
          compiler->SynthEffort(SynthesisEffort::High);
        } else if (arg == "medium") {
          compiler->SynthEffort(SynthesisEffort::Medium);
        } else if (arg == "low") {
          compiler->SynthEffort(SynthesisEffort::Low);
        } else {
          compiler->ErrorMessage("Unknown effort option: " + arg);
          return TCL_ERROR;
        }
        continue;
      }
      if (option == "-carry" && i + 1 < argc) {
        std::string arg = argv[++i];
        if (arg == "none") {
          compiler->SynthCarry(SynthesisCarryInference::NoCarry);
        } else if (arg == "auto") {
          compiler->SynthCarry(SynthesisCarryInference::Auto);
        } else if (arg == "all") {
          compiler->SynthCarry(SynthesisCarryInference::All);
        } else {
          compiler->ErrorMessage("Unknown carry inference option: " + arg);
          return TCL_ERROR;
        }
        continue;
      }
      if (option == "-no_dsp") {
        compiler->SynthNoDsp(true);
        continue;
      }
      if (option == "-no_bram") {
        compiler->SynthNoBram(true);
        continue;
      }
      if (option == "-no_adder") {
        compiler->SynthNoAdder(true);
        continue;
      }
      if (option == "-fast") {
        compiler->SynthFast(true);
        continue;
      }
      if (option == "-no_simplify") {
        compiler->SynthNoSimplify(true);
        continue;
      }
      if (option == "-clke_strategy" && i + 1 < argc) {
        std::string arg = argv[++i];
        if (arg == "early") {
          compiler->SynthClke(SynthesisClkeStrategy::Early);
        } else if (arg == "late") {
          compiler->SynthClke(SynthesisClkeStrategy::Late);
        } else {
          compiler->ErrorMessage("Unknown clock enable strategy: " + arg);
          return TCL_ERROR;
        }
        continue;
      }
      if (option == "-cec") {
        compiler->SynthCec(true);
        continue;
      }
      compiler->ErrorMessage("Unknown option: " + option);
      return TCL_ERROR;
    }
    return TCL_OK;
  };
  interp->registerCmd("synth_options", synth_options, this, 0);
  auto max_threads = [](void* clientData, Tcl_Interp* interp, int argc,
                        const char* argv[]) -> int {
    CompilerRS* compiler = (CompilerRS*)clientData;
    if (argc != 2) {
      compiler->ErrorMessage("Specify a number of threads.");
      return TCL_ERROR;
    }
    try {
      compiler->MaxThreads(std::stoi(std::string(argv[1])));
    } catch (...) {
      compiler->ErrorMessage("Specify integer as a number of threads.");
      return TCL_ERROR;
    }
    return TCL_OK;
  };
  interp->registerCmd("max_threads", max_threads, this, 0);

  return true;
}

std::string CompilerRS::BaseVprCommand() {
  std::string device_size = "";
  if (!m_deviceSize.empty()) {
    device_size = " --device " + m_deviceSize;
  }

  std::string netlistFile =
      m_projManager->projectName() +
      ((m_useVerilogNetlist) ? "_post_synth.v" : "_post_synth.blif");
  for (const auto& lang_file : m_projManager->DesignFiles()) {
    switch (lang_file.first) {
      case Design::Language::VERILOG_NETLIST:
      case Design::Language::BLIF:
      case Design::Language::EBLIF: {
        netlistFile = lang_file.second;
        std::filesystem::path the_path = netlistFile;
        if (!the_path.is_absolute()) {
          netlistFile =
              std::filesystem::path(std::filesystem::path("..") / netlistFile)
                  .string();
        }
        break;
      }
      default:
        break;
    }
  }

  std::string pnrOptions;
  if (!PnROpt().empty()) pnrOptions = " " + PnROpt();
  if (!PerDevicePnROptions().empty()) pnrOptions += " " + PerDevicePnROptions();
  std::string command =
      m_vprExecutablePath.string() + std::string(" ") +
      m_architectureFile.string() + std::string(" ") +
      std::string(
          netlistFile + std::string(" --sdc_file ") +
          std::string(m_projManager->projectName() + "_openfpga.sdc") +
          std::string(" --route_chan_width ") +
          std::to_string(m_channel_width) +
          " --suppress_warnings check_rr_node_warnings.log,check_rr_node"
          " --clock_modeling ideal --timing_report_npaths 100 "
          "--absorb_buffer_luts off --constant_net_method route "
          "--timing_report_detail detailed --post_place_timing_report " +
          m_projManager->projectName() + "_post_place_timing.rpt" +
          device_size + pnrOptions);

  return command;
}

void CompilerRS::Version(std::ostream* out) {
  (*out) << "Rapid Silicon Raptor Compiler"
         << "\n";
  PrintVersion(out);
}

void CompilerRS::Help(std::ostream* out) {
  (*out) << "-----------------------------------" << std::endl;
  (*out) << "--- Rapid Silicon RAPTOR HELP  ----" << std::endl;
  (*out) << "-----------------------------------" << std::endl;
  (*out) << "Options:" << std::endl;
  (*out) << "   --help           : This help" << std::endl;
  (*out) << "   --version        : Version" << std::endl;
  (*out) << "   --batch          : Tcl only, no GUI" << std::endl;
  (*out) << "   --script <script>: Execute a Tcl script" << std::endl;
  (*out) << "   --mute           : mutes stdout in batch mode" << std::endl;
  (*out) << "Tcl commands (Available in GUI or Batch console or Batch script):"
         << std::endl;
  (*out) << "   help                       : This help" << std::endl;
  (*out) << "   create_design <name>       : Creates a design with <name> name"
         << std::endl;
  (*out) << "   target_device <name>       : Targets a device with <name> name "
            "(MPW1, GEMINI)"
         << std::endl;
  (*out) << "   add_design_file <file>... <type> (-VHDL_1987, -VHDL_1993, "
            "-VHDL_2000, "
            "-VHDL_2008 (.vhd default), -V_1995, \n"
            "                                     -V_2001 (.v default), "
            "-SV_2005, -SV_2009, -SV_2012, -SV_2017 (.sv default)) "
         << std::endl;
  (*out) << "   read_netlist <file>        : Read a netlist (.blif/.eblif) "
            "instead of an RTL design (Skip Synthesis)"
         << std::endl;
  (*out) << "   add_include_path <path1>...: As in +incdir+    (Not applicable "
            "to VHDL)"
         << std::endl;
  (*out) << "   add_library_path <path1>...: As in +libdir+    (Not applicable "
            "to VHDL)"
         << std::endl;
  (*out) << "   add_library_ext <.v> <.sv> ...: As in +libext+ (Not applicable "
            "to VHDL)"
         << std::endl;
  (*out) << "   set_macro <name>=<value>...: As in -D<macro>=<value>"
         << std::endl;
  (*out) << "   set_top_module <top>       : Sets the top module" << std::endl;
  (*out) << "   add_constraint_file <file> : Sets SDC + location constraints"
         << std::endl;
  (*out) << "                                Constraints: set_pin_loc, "
            "all SDC Standard commands"
         << std::endl;
  (*out) << "   set_pin_loc <design_io_name> <device_io_name> : Constraints "
            "pin location (Use in constraint file)"
         << std::endl;
  (*out) << "   add_litex_ip_catalog <directory> : Browses directory for LiteX "
            "IP generators, adds the IP(s) to the IP Catalog"
         << std::endl;
  (*out) << "   ip_catalog ?<ip_name>?     : Lists all available IPs, and "
            "their parameters if <ip_name> is given "
         << std::endl;
  (*out) << "   ip_configure <IP_NAME> -mod_name <name> -out_file <filename> "
            "-version <ver_name> -P<param>=\"<value>\"..."
         << std::endl;
  (*out) << "                              : Configures an IP <IP_NAME> and "
            "generates the corresponding file with module name"
         << std::endl;
  (*out) << "   ipgenerate ?clean?         : Generates all IP instances set by "
            "ip_configure"
         << std::endl;
  (*out) << "   verific_parser <on/off>    : Turns on/off Verific Parser"
         << std::endl;
  (*out) << "   synthesis_type Yosys/QL/RS : Selects Synthesis type"
         << std::endl;
  (*out) << "   custom_synth_script <file> : Uses a custom Yosys templatized "
            "script"
         << std::endl;
  (*out) << "   max_threads <-1/[2:64]>    : Maximum number of threads to"
            " be used (-1 is for automatic selection)"
         << std::endl;
  (*out) << "   synth_options <option list>: RS-Yosys Plugin Options. "
            "The following defaults exist:"
         << std::endl;
  (*out) << "                              :   -effort high" << std::endl;
  (*out) << "                              :   -fsm_encoding binary if "
            "optimization == area else onehot"
         << std::endl;
  (*out) << "                              :   -carry auto" << std::endl;
  (*out) << "                              :   -clke_strategy early"
         << std::endl;
  (*out) << "     -effort <level>          : Optimization effort level (high,"
            " medium, low)"
         << std::endl;
  (*out) << "     -fsm_encoding <encoding> : FSM encoding:" << std::endl;
  (*out) << "       binary                 : Compact encoding - using minimum "
            "of registers to cover the N states"
         << std::endl;
  (*out) << "       onehot                 : One hot encoding - using N "
            "registers for N states"
         << std::endl;
  (*out) << "     -carry <mode>            : Carry logic inference mode:"
         << std::endl;
  (*out) << "       all                    : Infer as much as possible"
         << std::endl;
  (*out) << "       auto                   : Infer carries based on internal "
            "heuristics"
         << std::endl;
  (*out) << "       none                   : Do not infer carries" << std::endl;
  (*out) << "     -no_dsp                  : Do not use DSP blocks to "
            "implement multipliers and associated logic"
         << std::endl;
  (*out) << "     -no_bram                 : Do not use Block RAM to "
            "implement memory components"
         << std::endl;
  (*out) << "     -fast                    : Perform the fastest synthesis. "
            "Don't expect good QoR."
         << std::endl;
  (*out) << "     -no_simplify             : Do not run special "
            "simplification algorithms in synthesis. "
         << std::endl;
  (*out) << "     -clke_strategy <strategy>: Clock enable extraction "
            "strategy for FFs:"
         << std::endl;
  (*out) << "       early                  : Perform early extraction"
         << std::endl;
  (*out) << "       late                   : Perform late extraction"
         << std::endl;
  (*out) << "     -cec                     : Dump verilog after key phases "
            "and use internal equivalence checking (ABC based)"
         << std::endl;
  (*out) << "   synthesize <optimization>  ?clean? : RTL Synthesis, optional "
            "opt. (area, "
            "delay, mixed, none)"
         << std::endl;
  (*out) << "   pnr_options <option list>  : VPR options" << std::endl;
  (*out) << "   set_channel_width <int>    : VPR Routing channel setting"
         << std::endl;
  (*out) << "   architecture <vpr_file.xml> ?<openfpga_file.xml>?" << std::endl;
  (*out) << "                              : Uses the architecture file and "
            "optional openfpga arch file (For bitstream generation)"
         << std::endl;
  (*out) << "   custom_openfpga_script <file> : Uses a custom OpenFPGA "
            "templatized script"
         << std::endl;
  (*out) << "   bitstream_config_files -bitstream <bitstream_setting.xml> "
            "-sim <sim_setting.xml> -repack <repack_setting.xml>"
         << std::endl;
  (*out) << "                              : Uses alternate bitstream "
            "generation configuration files"
         << std::endl;
  (*out) << "   set_device_size XxY        : Device fabric size selection"
         << std::endl;
  (*out) << "   packing ?clean?            : Packing" << std::endl;
  (*out) << "   global_placement ?clean?   : Analytical placer" << std::endl;
  (*out) << "   place ?clean?              : Detailed placer" << std::endl;
  (*out) << "   route ?clean?              : Router" << std::endl;
  (*out) << "   sta ?clean?                : Statistical Timing Analysis"
         << std::endl;
  (*out) << "   power ?clean?              : Power estimator" << std::endl;
#ifdef PRODUCTION_BUILD
  (*out) << "   bitstream ?clean?          : Bitstream generation" << std::endl;
#else
  (*out) << "   bitstream ?force? ?clean?  : Bitstream generation" << std::endl;
#endif
  (*out) << "----------------------------------" << std::endl;
}

bool CompilerRS::LicenseDevice(const std::string& deviceName) {
  // Should return false in Production build if the Device License Feature
  // cannot be check out.
#ifdef PRODUCTION_BUILD
  try {
    auto license = License_Manager(deviceName);
  } catch (...) {
    return false;
  }

#endif
  return true;
}
