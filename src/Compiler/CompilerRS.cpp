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
#include "Compiler/Log.h"
#include "Configuration/CFGCompiler/CFGCompiler.h"
#include "ConfigurationRS/BitAssembler/BitAssembler.h"
#include "Main/Settings.h"
#include "MainWindow/Session.h"
#include "NewProject/ProjectManager/project_manager.h"
#include "Utils/FileUtils.h"
#include "Utils/LogUtils.h"
#include "Utils/StringUtils.h"

#ifdef PRODUCTION_BUILD
#include "License_manager.hpp"
#endif

extern FOEDAG::Session *GlobalSession;

using namespace FOEDAG;

const std::string QLYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -family ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${NO_DSP} ${NO_BRAM} ${FSM_ENCODING} -blif ${OUTPUT_BLIF} ${FAST} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}


write_verilog -noattr -nohex ${OUTPUT_VERILOG}
write_edif ${OUTPUT_EDIF}
write_blif -param ${OUTPUT_EBLIF}
  )";

const std::string RapidSiliconYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
read -vlog2k ${PRIMITIVES_BLACKBOX}
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} ${ABC_SCRIPT} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${LIMITS} ${NO_DSP} ${NO_BRAM} ${FSM_ENCODING} ${FAST} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

  )";

static auto assembler_flow(CompilerRS *compiler, bool batchMode, int argc,
                           const char *argv[]) {
  CFGCompiler *cfgcompiler = compiler->GetConfiguration();
  compiler->BitsOpt(Compiler::BitstreamOpt::DefaultBitsOpt);
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "force") {
      compiler->BitsOpt(Compiler::BitstreamOpt::Force);
    } else if (arg == "clean") {
      compiler->BitsOpt(Compiler::BitstreamOpt::Clean);
    } else if (arg == "enable_simulation") {
      compiler->BitsOpt(Compiler::BitstreamOpt::EnableSimulation);
    } else {
      compiler->ErrorMessage("Unknown bitstream option: " + arg);
      return TCL_ERROR;
    }
  }
  cfgcompiler->m_cmdarg.command = "assembler";
  cfgcompiler->m_cmdarg.clean =
      compiler->BitsOpt() == Compiler::BitstreamOpt::Clean;

  // Call Compile()
  if (batchMode) {
    if (!compiler->Compile(Compiler::Action::Bitstream)) {
      return TCL_ERROR;
    }
  } else {
    WorkerThread *wthread =
        new WorkerThread("bitstream_th", Compiler::Action::Bitstream, compiler);
    if (!wthread->start()) {
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

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

std::string CompilerRS::FinishAnalyzeScript(const std::string &script) {
  std::filesystem::path datapath =
      GlobalSession->Context()->DataPath().parent_path();
  std::filesystem::path tech_datapath =
      datapath / "raptor" / "sim_models" / "rapidsilicon" / m_mapToTechnology;
  std::filesystem::path primitivesBlackboxPath =
      tech_datapath / "cell_sim_blackbox.v";
  std::string result = "-sv2009 " + primitivesBlackboxPath.string() + "\n";
  result += script;
  return result;
}

std::string CompilerRS::FinishSynthesisScript(const std::string &script) {
  std::string result = script;
  std::filesystem::path datapath =
      GlobalSession->Context()->DataPath().parent_path();
  std::filesystem::path tech_datapath =
      datapath / "raptor" / "sim_models" / "rapidsilicon" / m_mapToTechnology;
  std::filesystem::path primitivesBlackboxPath =
      tech_datapath / "cell_sim_blackbox.v";
  result = ReplaceAll(result, "${PRIMITIVES_BLACKBOX}",
                      primitivesBlackboxPath.string());
  // Keeps for Synthesis, preserve nodes used in constraints
  std::string keeps;
  if (m_keepAllSignals) {
    keeps += "setattr -set keep 1 w:\\*\n";
  }
  for (auto keep : m_constraints->GetKeeps()) {
    keep = ReplaceAll(keep, "@", "[");
    keep = ReplaceAll(keep, "%", "]");
    // Message("Keep name: " + keep);
    keeps += "setattr -set keep 1 w:\\" + keep + "\n";
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

  std::string limits;
  limits += std::string("-max_device_dsp ") +
            std::to_string(MaxDeviceDSPCount()) + std::string(" ");
  limits += std::string("-max_device_bram ") +
            std::to_string(MaxDeviceBRAMCount()) + std::string(" ");
  if (MaxUserDSPCount())
    limits += std::string("-max_dsp ") + std::to_string(MaxUserDSPCount()) +
              std::string(" ");
  if (MaxUserBRAMCount())
    limits += std::string("-max_bram ") + std::to_string(MaxUserBRAMCount()) +
              std::string(" ");

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
  result = ReplaceAll(result, "${LIMITS}", limits);
  result = ReplaceAll(result, "${OPTIMIZATION}", optimization);
  result = ReplaceAll(result, "${EFFORT}", effort);
  result = ReplaceAll(result, "${FSM_ENCODING}", fsm_encoding);
  result = ReplaceAll(result, "${CARRY}", carry_inference);
  result = ReplaceAll(result, "${NO_DSP}", {});
  result = ReplaceAll(result, "${NO_BRAM}", {});
  result = ReplaceAll(result, "${FAST}", fast_mode);
  result = ReplaceAll(result, "${MAX_THREADS}", max_threads);
  result = ReplaceAll(result, "${NO_SIMPLIFY}", no_simplify);
  result = ReplaceAll(result, "${CLKE_STRATEGY}", clke_strategy);
  result = ReplaceAll(result, "${CEC}", cec);
  result = ReplaceAll(result, "${PLUGIN_LIB}", YosysPluginLibName());
  result = ReplaceAll(result, "${PLUGIN_NAME}", YosysPluginName());
  result = ReplaceAll(result, "${MAP_TO_TECHNOLOGY}", YosysMapTechnology());

  if ((m_mapToTechnology == "genesis2") && getenv("GENESIS2_LUT5")) {
    // Hack for genesis2 untill we fix the LUT6 packing, this disables all
    // optimizations.
    result = ReplaceAll(result, "${LUT_SIZE}", std::to_string(5));
    result = ReplaceAll(result, "${ABC_SCRIPT}", "-abc lut.scr");
    std::filesystem::path scr =
        std::filesystem::path(ProjManager()->projectPath()) / "lut.scr";
    std::ofstream ofs(scr.string());
    ofs << "if -K 5 -a\n";
    ofs.close();
  } else {
    result = ReplaceAll(result, "${ABC_SCRIPT}", "");
  }

  result = ReplaceAll(result, "${LUT_SIZE}", std::to_string(m_lut_size));
  switch (GetNetlistType()) {
    case NetlistType::Verilog:
      // Temporary, once pin_c works with Verilog, only output Verilog
      result = ReplaceAll(result, "${OUTPUT_NETLIST}",
                          "write_verilog -noexpr -nodec -v "
                          "${OUTPUT_VERILOG}\nwrite_blif ${OUTPUT_BLIF}");
      break;
    case NetlistType::VHDL:
      // Temporary, once pin_c and the Packer work with VHDL, replace by just
      // VHDL
      result = ReplaceAll(result, "${OUTPUT_NETLIST}",
                          "write_vhdl ${OUTPUT_VHDL}\nwrite_verilog "
                          "${OUTPUT_VERILOG}\nwrite_blif ${OUTPUT_BLIF}");
      break;
    case NetlistType::Edif:
      // Temporary, once pin_c works with Verilog, only output edif
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_edif ${OUTPUT_EDIF}\nwrite_blif ${OUTPUT_BLIF}");
      break;
    case NetlistType::Blif:
      result = ReplaceAll(result, "${OUTPUT_NETLIST}",
                          "write_verilog -noexpr -nodec -v "
                          "${OUTPUT_VERILOG}\nwrite_blif ${OUTPUT_BLIF}");
      break;
    case NetlistType::EBlif:
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_verilog -noexpr -nodec -norename "
                     "${OUTPUT_VERILOG}\nwrite_blif -param ${OUTPUT_EBLIF}");
      break;
  }

  return result;
}

CompilerRS::CompilerRS() : CompilerOpenFPGA() {
  m_synthType = SynthesisType::RS;
  m_netlistType = NetlistType::Verilog;
  m_channel_width = 200;
}

void CompilerRS::CustomSimulatorSetup(Simulator::SimulationType action) {
  std::filesystem::path datapath =
      GlobalSession->Context()->DataPath().parent_path();
  std::filesystem::path tech_datapath =
      datapath / "raptor" / "sim_models" / "rapidsilicon" / m_mapToTechnology;
  GetSimulator()->ResetGateSimulationModel();
  switch (GetNetlistType()) {
    case NetlistType::Verilog:
      if (action == Simulator::SimulationType::Gate ||
          action == Simulator::SimulationType::PNR) {
        GetSimulator()->AddGateSimulationModel(tech_datapath / "cells_sim.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "simlib.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "dsp_sim.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "brams_sim.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "TDP18K_FIFO.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "ufifo_ctl.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "sram1024x18.v");
      }
      if (action == Simulator::SimulationType::PNR) {
        GetSimulator()->AddGateSimulationModel(tech_datapath / "primitives.v");
      }
      break;
    case NetlistType::VHDL:
      GetSimulator()->AddGateSimulationModel(tech_datapath / "cells_sim.vhd");
      break;
    case NetlistType::Edif:
      break;
    case NetlistType::Blif:
      break;
    case NetlistType::EBlif:
      break;
  }
}

CompilerRS::~CompilerRS() {
#ifdef PRODUCTION_BUILD
  if (licensePtr) delete licensePtr;
#endif
}

bool CompilerRS::RegisterCommands(TclInterpreter *interp, bool batchMode) {
  CompilerOpenFPGA::RegisterCommands(interp, batchMode);

  auto synth_options = [](void *clientData, Tcl_Interp *interp, int argc,
                          const char *argv[]) -> int {
    CompilerRS *compiler = (CompilerRS *)clientData;
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
      if (option == "-dsp_limit" && i + 1 < argc) {
        std::string arg = argv[++i];
        const auto &[value, ok] = StringUtils::to_number<uint32_t>(arg);
        if (ok) compiler->MaxUserDSPCount(value);
        continue;
      }
      if (option == "-bram_limit" && i + 1 < argc) {
        std::string arg = argv[++i];
        const auto &[value, ok] = StringUtils::to_number<uint32_t>(arg);
        if (ok) compiler->MaxUserBRAMCount(value);
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
  auto max_threads = [](void *clientData, Tcl_Interp *interp, int argc,
                        const char *argv[]) -> int {
    CompilerRS *compiler = (CompilerRS *)clientData;
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

  // Configuration
  CFGCompiler *cfgcompiler = GetConfiguration();
  if (batchMode) {
    auto assembler = [](void *clientData, Tcl_Interp *interp, int argc,
                        const char *argv[]) -> int {
      CompilerRS *compiler = (CompilerRS *)clientData;
      CFGCompiler *cfgcompiler = compiler->GetConfiguration();
      int status = TCL_OK;
      if ((status = assembler_flow(compiler, true, argc, argv)) != TCL_OK) {
        return CFGCompiler::Compile(cfgcompiler, true);
      } else {
        return status;
      }
    };
    interp->registerCmd("assembler", assembler, this, 0);
  } else {
    auto assembler = [](void *clientData, Tcl_Interp *interp, int argc,
                        const char *argv[]) -> int {
      CompilerRS *compiler = (CompilerRS *)clientData;
      CFGCompiler *cfgcompiler = compiler->GetConfiguration();
      int status = TCL_OK;
      if ((status = assembler_flow(compiler, false, argc, argv)) == TCL_OK) {
        return CFGCompiler::Compile(cfgcompiler, false);
      } else {
        return status;
      }
    };
    interp->registerCmd("assembler", assembler, this, 0);
  }
  cfgcompiler->RegisterCallbackFunction("assembler", BitAssembler_entry);
  return true;
}

std::string CompilerRS::BaseVprCommand() {
  std::string device_size = "";
  if (!m_deviceSize.empty()) {
    device_size = " --device " + m_deviceSize;
  }
  std::string netlistFile;
  switch (GetNetlistType()) {
    case NetlistType::Verilog:
      netlistFile = ProjManager()->projectName() + "_post_synth.v";
      break;
    case NetlistType::VHDL:
      netlistFile = ProjManager()->projectName() + "_post_synth.v";
      break;
    case NetlistType::Edif:
      netlistFile = ProjManager()->projectName() + "_post_synth.edif";
      break;
    case NetlistType::Blif:
      netlistFile = ProjManager()->projectName() + "_post_synth.blif";
      break;
    case NetlistType::EBlif:
      netlistFile = ProjManager()->projectName() + "_post_synth.eblif";
      break;
  }
  for (const auto &lang_file : m_projManager->DesignFiles()) {
    switch (lang_file.first.language) {
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
  if (ClbPackingOption() == ClbPacking::Auto) {
    pnrOptions += " --allow_unrelated_clustering off";
  } else {
    pnrOptions += " --allow_unrelated_clustering on";
  }
  if (!PnROpt().empty()) pnrOptions += " " + PnROpt();
  if (pnrOptions.find("gen_post_synthesis_netlist") == std::string::npos) {
    pnrOptions += " --gen_post_synthesis_netlist on";
  }
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
          "--absorb_buffer_luts off --skip_sync_clustering_and_routing_results "
          "on --constant_net_method route "
          "--timing_report_detail detailed --post_place_timing_report " +
          m_projManager->projectName() + "_post_place_timing.rpt" +
          device_size + pnrOptions);

  return command;
}

void CompilerRS::Version(std::ostream *out) {
  (*out) << "Rapid Silicon Raptor Design Suite"
         << "\n";
  LogUtils::PrintVersion(out);
}

void CompilerRS::Help(std::ostream *out) {
  (*out) << "-----------------------------------------------" << std::endl;
  (*out) << "--- Rapid Silicon Raptor Design Suite help  ---" << std::endl;
  (*out) << "-----------------------------------------------" << std::endl;
  (*out) << "Options:" << std::endl;
  (*out) << "   --help           : This help" << std::endl;
  (*out) << "   --version        : Version" << std::endl;
  (*out) << "   --batch          : Tcl only, no GUI" << std::endl;
  (*out) << "   --script <script>: Execute a Tcl script" << std::endl;
  (*out) << "   --project <project file>: Open a project" << std::endl;
  (*out) << "   --mute           : mutes stdout in batch mode" << std::endl;
  (*out) << "Tcl commands (Available in GUI or Batch console or Batch script):"
         << std::endl;
  (*out) << "   help                       : This help" << std::endl;
  (*out) << "   open_project <file>        : Opens a project in the GUI"
         << std::endl;
  (*out) << "   run_project <file>         : Opens and immediately runs the "
            "project"
         << std::endl;
  (*out) << "   create_design <name> ?-type <project type>? : Creates a design "
            "with <name> name"
         << std::endl;
  (*out) << "               <project type> : rtl (Default), gate-level"
         << std::endl;
  (*out) << "   target_device <name>       : Targets a device with <name> name "
            "(1GE75)"
         << std::endl;
  (*out) << "   add_design_file <file list> ?type?   ?-work <libName>?   ?-L "
            "<libName>? "
         << std::endl;
  (*out) << "              Each invocation of the command compiles the "
            "file list into a compilation unit "
         << std::endl;
  (*out) << "                       <type> : -VHDL_1987, -VHDL_1993, "
            "-VHDL_2000, -VHDL_2008, -VHDL_2019, -V_1995, "
            "-V_2001, -SV_2005, -SV_2009, -SV_2012, -SV_2017> "
         << std::endl;
  (*out) << "              -work <libName> : Compiles the compilation unit "
            "into library <libName>, default is \"work\""
         << std::endl;
  (*out) << "              -L <libName>    : Import the library <libName> "
            "needed to "
            "compile the compilation unit, default is \"work\""
         << std::endl;
  (*out) << "   add_simulation_file <file list> ?type?   ?-work <libName>?   "
            "?-L <libName>? "
         << std::endl;
  (*out) << "              Each invocation of the command compiles the "
            "file list into a compilation unit "
         << std::endl;
  (*out) << "                       <type> : -VHDL_1987, -VHDL_1993, "
            "-VHDL_2000, -VHDL_2008, -VHDL_2019, -V_1995, "
            "-V_2001, -SV_2005, -SV_2009, -SV_2012, -SV_2017, -C, -CPP> "
         << std::endl;
  (*out) << "              -work <libName> : Compiles the compilation unit "
            "into library <libName>, default is \"work\""
         << std::endl;
  (*out) << "              -L <libName>    : Import the library <libName> "
            "needed to "
            "compile the compilation unit, default is \"work\""
         << std::endl;
  (*out) << "   clear_simulation_files     : Remove all simulation files"
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
  (*out) << "   set_top_module <top> ?-work <libName>? : Sets the top module"
         << std::endl;
  (*out) << "   add_constraint_file <file> : Sets SDC + location constraints"
         << std::endl;
  (*out)
      << "                                Constraints: set_pin_loc, set_mode, "
         "all SDC Standard commands"
      << std::endl;
  (*out) << "   set_pin_loc <design_io_name> <device_io_name> "
            "?<internal_pinname>?: Constraints "
            "pin location (Use in constraint.pin file)"
         << std::endl;
  (*out)
      << "   set_property mode <io_mode_name> <device_io_name> : Constraints "
         "pin mode (Use in constraint.pin file)"
      << std::endl;
  (*out) << "   set_clock_pin -device_clock <device_clock_name> -design_clock "
            "<design_clock_name>"
         << std::endl;
  (*out) << "                              : like gemini has 16 clocks "
            "clk[0],clk[1]....,clk[15]."
         << std::endl;
  (*out) << "                                and e.g. user clocks are clk_a, "
            "clk_b and want to map clk_a with clk[15]"
         << std::endl;
  (*out)
      << "                               Constraints : set clocks for repacking"
         " constraints (Use in constraint.pin file)"
      << std::endl;
  (*out) << "   script_path                : Returns the path of the Tcl "
            "script passed "
            "with --script"
         << std::endl;
  (*out) << "   keep <signal list> OR all_signals : Keeps the list of signals "
            "or all signals through Synthesis unchanged (unoptimized in "
            "certain cases)"
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
  (*out) << "   message_severity <message_id> <ERROR/WARNING/INFO/IGNORE> : "
            "Upgrade/downgrade RTL compilation message severity"
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
  (*out) << "   analyze ?clean?            : Analyzes the RTL design, "
            "generates top-level, pin and hierarchy information"
         << std::endl;
  (*out) << "   synthesize <optimization>  ?clean? : RTL Synthesis, optional "
            "opt. (area, "
            "delay, mixed, none)"
         << std::endl;
  (*out) << "   pin_loc_assign_method <Method>: Method choices:" << std::endl;
  (*out) << "                                in_define_order(Default), port "
            "order pin assignment"
         << std::endl;
  (*out) << "                                random , random pin assignment"
         << std::endl;
  (*out) << "                                free , no automatic pin assignment"
         << std::endl;
  (*out) << "   pnr_options <option list>  : VPR options" << std::endl;
  (*out) << "   clb_packing   <directive>  : Performance optimization flags"
         << std::endl;
  (*out) << "                 <directive>  : auto, dense" << std::endl;
  (*out) << "                        auto  : CLB packing automatically "
            "determined to optimize performance"
         << std::endl;
  (*out)
      << "                       dense  : Pack logic more densely into CLBs "
         "resulting in fewer utilized CLBs however may negatively impact timing"
      << std::endl;
  (*out) << "   pnr_netlist_lang <blif, edif, verilog, vhdl> : Chooses "
            "post-synthesis "
            "netlist format"
         << std::endl;
  (*out) << "   set_channel_width <int>    : VPR Routing channel setting"
         << std::endl;
  (*out) << "   set_limits <type> <int>    : Sets a user limit on object of "
            "type <type>"
         << std::endl;
  (*out) << "                       <type> : dsp, bram" << std::endl;
  (*out) << "                          dsp : Max usable DSPs" << std::endl;
  (*out) << "                         bram : Max usable BRAMs" << std::endl;
  (*out) << "   architecture <vpr_file.xml> ?<openfpga_file.xml>?" << std::endl;
  (*out) << "                              : Uses the architecture file and "
            "optional openfpga arch file (For bitstream generation)"
         << std::endl;
  (*out) << "   custom_openfpga_script <file> : Uses a custom OpenFPGA "
            "templatized script"
         << std::endl;
  (*out) << "   bitstream_config_files -bitstream <bitstream_setting.xml> "
            "-sim <sim_setting.xml> -repack <repack_setting.xml> -key "
            "<fabric_key.xml>"
         << std::endl;
  (*out) << "                              : Uses alternate bitstream "
            "generation configuration files"
         << std::endl;
  (*out) << "   set_device_size XxY        : Device fabric size selection"
         << std::endl;
  (*out) << "   packing ?clean?            : Packing" << std::endl;
  // (*out) << "   global_placement ?clean?   : Analytical placer" << std::endl;
  (*out) << "   place ?clean?              : Detailed placer" << std::endl;
  (*out) << "   route ?clean?              : Router" << std::endl;
#ifdef PRODUCTION_BUILD
  (*out) << "   sta ?clean?                : Statistical Timing Analysis"
         << std::endl;
#else
  (*out) << "   sta ?clean? ?opensta?      : Statistical Timing Analysis with "
            "Tatum (Default) or OpenSTA"
         << std::endl;
#endif
//  (*out) << "   power ?clean?              : Power estimator" << std::endl;
#ifdef PRODUCTION_BUILD
  (*out) << "   bitstream ?clean?          : Bitstream generation" << std::endl;
#else
  (*out) << "   bitstream ?force? ?clean?  : Bitstream generation" << std::endl;
#endif
  (*out) << "   set_top_testbench <module> : Sets the top-level testbench "
            "module/entity"
         << std::endl;
  (*out) << "   simulation_options <simulator> <phase> ?<level>? <options>"
         << std::endl;
  (*out) << "                                Sets the simulator specific "
            "options for the specified phase"
         << std::endl;
  (*out) << "                      <phase> : compilation, elaboration, "
            "simulation, extra_options"
         << std::endl;
  (*out) << "   simulate <level> ?<simulator>? ?<waveform_file>?: Simulates "
            "the design and testbench"
         << std::endl;
  (*out)
      << "                      <level> : rtl, gate, pnr. rtl: RTL simulation, "
         "gate: post-synthesis simulation, pnr: post-pnr simulation"
      << std::endl;
  (*out) << "                  <simulator> : verilator, ghdl, icarus"
         << std::endl;
  writeWaveHelp(out, 3, 30);  // 30 is the col count of the : in the line above
  (*out) << "-----------------------------------------------" << std::endl;
}

bool CompilerRS::LicenseDevice(const std::string &deviceName) {
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

// This collects the current compiler values and returns a string of args that
// will be used by the settings UI to populate its fields
std::string FOEDAG::TclArgs_getRsSynthesisOptions() {
  CompilerRS *compiler = (CompilerRS *)GlobalSession->GetCompiler();
  std::string tclOptions{};
  if (compiler) {
    // Use the script completer to generate an arg list w/ current values
    // Note we are only grabbing the values that matter for the settings ui
    tclOptions = compiler->FinishSynthesisScript(
        "${OPTIMIZATION} ${EFFORT} ${FSM_ENCODING} ${CARRY} ${NO_DSP} "
        "${NO_BRAM} ${FAST}");

    // The settings UI provides a single argument value pair for each field.
    // Optimization uses -de -goal so we convert that to the settings specific
    // synth arg, -_SynthOpt_
    std::string tag = "-_SynthOpt_";
    std::string noneStr = tag + " none";
    // generically replace -de -goal * with -_SynthOpt_ *
    tclOptions = StringUtils::replaceAll(tclOptions, "-de -goal", tag);
    // special case for None, must be run AFTER -de -goal has been replaced
    tclOptions = StringUtils::replaceAll(tclOptions, "-de", noneStr);

    // short term fix for carry which currently seems to use both no and none
    tclOptions =
        StringUtils::replaceAll(tclOptions, "-carry no ", "-carry none ");

    tclOptions += " -netlist_lang ";
    switch (compiler->GetNetlistType()) {
      case Compiler::NetlistType::Blif:
        tclOptions += "blif";
        break;
      case Compiler::NetlistType::EBlif:
        tclOptions += "eblif";
        break;
      case Compiler::NetlistType::Edif:
        tclOptions += "edif";
        break;
      case Compiler::NetlistType::VHDL:
        tclOptions += "vhdl";
        break;
      case Compiler::NetlistType::Verilog:
        tclOptions += "verilog";
        break;
    }

    if (compiler->SynthEffort() == CompilerRS::SynthesisEffort::None) {
      tclOptions += " -effort none";
    }
    if (compiler->SynthFsm() == CompilerRS::SynthesisFsmEncoding::None) {
      tclOptions += " -fsm_encoding none";
    }
    auto dsp = StringUtils::to_string(compiler->MaxUserDSPCount());
    tclOptions += " -dsp_limit " + dsp;
    auto bram = StringUtils::to_string(compiler->MaxUserBRAMCount());
    tclOptions += " -bram_limit " + bram;
  };
  return tclOptions;
}

// This takes an arglist generated by the settings dialog and applies the
// options to CompilerRs
void FOEDAG::TclArgs_setRsSynthesisOptions(const std::string &argsStr) {
  CompilerRS *compiler = (CompilerRS *)GlobalSession->GetCompiler();

  bool fast{false};
  // Split into args by -
  std::vector<std::string> args;
  StringUtils::tokenize(argsStr, "-", args);
  for (auto arg : args) {
    // add back the "-" removed from tokenize()
    arg = "-" + arg;

    // Split arg into tokens
    std::vector<std::string> tokens;
    StringUtils::tokenize(arg, " ", tokens);

    // Read in arg tag
    std::string option{};
    if (tokens.size() > 0) {
      option = tokens[0];
    }

    // This codes mostly copies the logic from the synth_options lambda
    // It would be nice if the syth_options logic could be refactored into
    // pieces that could be used in this parser as well

    // -_SynthOpt_ is the settings dlg's arg for Optimization (-de -goal)
    if (option == "-_SynthOpt_" && tokens.size() > 1) {
      std::string arg = tokens[1];
      if (arg == "area") {
        compiler->SynthOpt(Compiler::SynthesisOpt::Area);
      } else if (arg == "delay") {
        compiler->SynthOpt(Compiler::SynthesisOpt::Delay);
      } else if (arg == "mixed") {
        compiler->SynthOpt(Compiler::SynthesisOpt::Mixed);
      } else if (arg == "none") {
        compiler->SynthOpt(Compiler::SynthesisOpt::None);
      }
      continue;
    }
    if (option == "-fsm_encoding" && tokens.size() > 1) {
      std::string arg = tokens[1];
      if (arg == "binary") {
        compiler->SynthFsm(CompilerRS::SynthesisFsmEncoding::Binary);
      } else if (arg == "onehot") {
        compiler->SynthFsm(CompilerRS::SynthesisFsmEncoding::Onehot);
      } else if (arg == "none") {
        compiler->SynthFsm(CompilerRS::SynthesisFsmEncoding::None);
      }
      continue;
    }
    if (option == "-effort" && tokens.size() > 1) {
      std::string arg = tokens[1];
      if (arg == "high") {
        compiler->SynthEffort(CompilerRS::SynthesisEffort::High);
      } else if (arg == "medium") {
        compiler->SynthEffort(CompilerRS::SynthesisEffort::Medium);
      } else if (arg == "low") {
        compiler->SynthEffort(CompilerRS::SynthesisEffort::Low);
      } else if (arg == "none") {
        compiler->SynthEffort(CompilerRS::SynthesisEffort::None);
      }
      continue;
    }
    if (option == "-netlist_lang" && tokens.size() > 1) {
      std::string arg = tokens[1];
      if (arg == "blif") {
        compiler->SetNetlistType(Compiler::NetlistType::Blif);
      } else if (arg == "eblif") {
        compiler->SetNetlistType(Compiler::NetlistType::EBlif);
      } else if (arg == "edif") {
        compiler->SetNetlistType(Compiler::NetlistType::Edif);
      } else if (arg == "vhdl") {
        compiler->SetNetlistType(Compiler::NetlistType::VHDL);
      } else if (arg == "verilog") {
        compiler->SetNetlistType(Compiler::NetlistType::Verilog);
      }
      continue;
    }
    if (option == "-carry" && tokens.size() > 1) {
      std::string arg = tokens[1];
      if (arg == "none") {
        compiler->SynthCarry(CompilerRS::SynthesisCarryInference::NoCarry);
      } else if (arg == "auto") {
        compiler->SynthCarry(CompilerRS::SynthesisCarryInference::Auto);
      } else if (arg == "all") {
        compiler->SynthCarry(CompilerRS::SynthesisCarryInference::All);
      } else if (arg == "<unset>") {
        compiler->SynthCarry(CompilerRS::SynthesisCarryInference::None);
      }
      continue;
    }
    if (option == "-dsp_limit" && tokens.size() > 1) {
      std::string arg = tokens[1];
      const auto &[value, ok] = StringUtils::to_number<uint32_t>(arg);
      if (ok) compiler->MaxUserDSPCount(value);
      continue;
    }
    if (option == "-bram_limit" && tokens.size() > 1) {
      std::string arg = tokens[1];
      const auto &[value, ok] = StringUtils::to_number<uint32_t>(arg);
      if (ok) compiler->MaxUserBRAMCount(value);
      continue;
    }
    if (option == "-fast") {
      fast = true;
      continue;
    }
  }
  compiler->SynthFast(fast);
  GlobalSession->GetSettings()->syncWith(PACKING_SETTING_KEY);
}

std::string CompilerRS::BaseStaCommand() {
  std::string command = m_staExecutablePath.string();
  return command;
}

std::string CompilerRS::BaseStaScript(std::string libFileName,
                                      std::string netlistFileName,
                                      std::string sdfFileName,
                                      std::string sdcFileName) {
  std::string script =
      std::string("read_liberty ") + libFileName +
      std::string("\n") +  // add lib for test only, need to research on this
      std::string("read_verilog ") + netlistFileName + std::string("\n") +
      std::string("link_design ") +
      ProjManager()->getDesignTopModule().toStdString() + std::string("\n") +
      std::string("read_sdf ") + sdfFileName + std::string("\n") +
      std::string("read_sdc ") + sdcFileName + std::string("\n") +
      std::string("report_checks\n") + std::string("report_wns\n") +
      std::string("report_tns\n") + std::string("exit\n");
  const std::string openStaFile =
      (std::filesystem::path(ProjManager()->projectPath()) /
       std::string(ProjManager()->projectName() + "_opensta.tcl"))
          .string();
  std::ofstream ofssta(openStaFile);
  ofssta << script << "\n";
  ofssta.close();
  return openStaFile;
}

bool CompilerRS::TimingAnalysis() {
  if (!ProjManager()->HasDesign()) {
    ErrorMessage("No design specified");
    return false;
  }
  if (!HasTargetDevice()) return false;

  if (TimingAnalysisOpt() == STAOpt::Clean) {
    Message("Cleaning TimingAnalysis results for " +
            ProjManager()->projectName());
    TimingAnalysisOpt(STAOpt::None);
    m_state = State::Routed;
    CleanFiles(Action::STA);
    return true;
  }

  PERF_LOG("TimingAnalysis has started");
  Message("##################################################");
  Message("Timing Analysis for design: " + ProjManager()->projectName());
  Message("##################################################");
  if (!FileUtils::FileExists(m_vprExecutablePath)) {
    ErrorMessage("Cannot find executable: " + m_vprExecutablePath.string());
    return false;
  }

  if (TimingAnalysisOpt() == STAOpt::View) {
    TimingAnalysisOpt(STAOpt::None);
    const std::string command = BaseVprCommand() + " --analysis --disp on";
    const int status = ExecuteAndMonitorSystemCommand(command);
    if (status) {
      ErrorMessage("Design " + ProjManager()->projectName() +
                   " place and route view failed!");
      return false;
    }
    return true;
  }

  if (FileUtils::IsUptoDate(
          (std::filesystem::path(ProjManager()->projectPath()) /
           std::string(ProjManager()->projectName() + "_post_synth.route"))
              .string(),
          (std::filesystem::path(ProjManager()->projectPath()) /
           std::string("timing_analysis.rpt"))
              .string())) {
    Message("Design " + ProjManager()->projectName() + " timing didn't change");
    return true;
  }
  int status = 0;
  std::string taCommand;
  // use OpenSTA to do the job
  if (TimingAnalysisEngineOpt() == STAEngineOpt::Opensta) {
    // allows SDF to be generated for OpenSTA

    // calls stars to generate files for opensta
    // stars pass all arguments to vpr to generate design context
    // then it does it work based on that
    std::string vpr_executable_path =
        m_vprExecutablePath.string() + std::string(" ");
    std::string command = BaseVprCommand();
    if (command.find(vpr_executable_path) != std::string::npos) {
      command = ReplaceAll(command, vpr_executable_path,
                           m_starsExecutablePath.string() + " ");
    }
    if (!FileUtils::FileExists(m_starsExecutablePath)) {
      ErrorMessage("Cannot find executable: " + m_starsExecutablePath.string());
      return false;
    }
    std::ofstream ofs((std::filesystem::path(ProjManager()->projectPath()) /
                       std::string(ProjManager()->projectName() + "_sta.cmd"))
                          .string());
    ofs << command << std::endl;
    ofs.close();
    int status = ExecuteAndMonitorSystemCommand(command);
    if (status) {
      ErrorMessage("Design " + ProjManager()->projectName() +
                   " timing analysis failed!");
      return false;
    }
    // find files
    std::string libFileName =
        (std::filesystem::path(ProjManager()->projectPath()) /
         std::string(ProjManager()->getDesignTopModule().toStdString() +
                     "_stars.lib"))
            .string();
    std::string netlistFileName =
        (std::filesystem::path(ProjManager()->projectPath()) /
         std::string(ProjManager()->getDesignTopModule().toStdString() +
                     "_stars.v"))
            .string();
    std::string sdfFileName =
        (std::filesystem::path(ProjManager()->projectPath()) /
         std::string(ProjManager()->getDesignTopModule().toStdString() +
                     "_stars.sdf"))
            .string();
    // std::string sdcFile = ProjManager()->getConstrFiles();
    std::string sdcFileName =
        (std::filesystem::path(ProjManager()->projectPath()) /
         std::string(ProjManager()->getDesignTopModule().toStdString() +
                     "_stars.sdc"))
            .string();
    if (std::filesystem::is_regular_file(libFileName) &&
        std::filesystem::is_regular_file(netlistFileName) &&
        std::filesystem::is_regular_file(sdfFileName) &&
        std::filesystem::is_regular_file(sdcFileName)) {
      taCommand =
          BaseStaCommand() + " " +
          BaseStaScript(libFileName, netlistFileName, sdfFileName, sdcFileName);
      std::ofstream ofs((std::filesystem::path(ProjManager()->projectPath()) /
                         std::string(ProjManager()->projectName() + "_sta.cmd"))
                            .string());
      ofs << taCommand << std::endl;
      ofs.close();
    } else {
      ErrorMessage(
          "No required design info generated for user design, required "
          "for timing analysis");
      return false;
    }
  } else {  // use vpr/tatum engine
    taCommand = BaseVprCommand() + " --analysis";
    std::ofstream ofs((std::filesystem::path(ProjManager()->projectPath()) /
                       std::string(ProjManager()->projectName() + "_sta.cmd"))
                          .string());
    ofs << taCommand << " --disp on" << std::endl;
    ofs.close();
  }

  status = ExecuteAndMonitorSystemCommand(taCommand, TIMING_ANALYSIS_LOG);
  if (status) {
    ErrorMessage("Design " + ProjManager()->projectName() +
                 " timing analysis failed!");
    return false;
  }

  Message("Design " + ProjManager()->projectName() + " is timing analysed!");

  return true;
}

std::vector<std::string> CompilerRS::GetCleanFiles(
    Action action, const std::string &projectName,
    const std::string &topModule) const {
  auto files = CompilerOpenFPGA::GetCleanFiles(action, projectName, topModule);
  if (action == Action::STA) {
    files.push_back(std::string{topModule + "_stars.lib"});
    files.push_back(std::string{topModule + "_stars.v"});
    files.push_back(std::string{topModule + "_stars.sdf"});
    files.push_back(std::string{topModule + "_stars.sdc"});
    files.push_back(std::string{projectName + "_opensta.tcl"});
  }
  return files;
}
