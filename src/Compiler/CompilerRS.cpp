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
#include "MainWindow/Session.h"
#include "NewProject/ProjectManager/project_manager.h"
#include "ProjNavigator/tcl_command_integration.h"
#include "Utils/FileUtils.h"
#include "Utils/LogUtils.h"
#include "Utils/StringUtils.h"
#include "scope_guard/scope_guard.hpp"

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

${PLUGIN_NAME} -family ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${FSM_ENCODING} -blif ${OUTPUT_BLIF} ${FAST} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}


write_verilog -noattr -nohex ${OUTPUT_VERILOG}
write_edif ${OUTPUT_EDIF}
write_blif -param ${OUTPUT_EBLIF}
  )";

const std::string RapidSiliconYosysVerificScript = R"( 
# Yosys/Verific synthesis script for ${TOP_MODULE}
# Read source files
verific -sv2005 ${PRIMITIVES_BLACKBOX}
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

  )";

const std::string RapidSiliconYosysDefaultScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
read_verilog -sv ${PRIMITIVES_BLACKBOX}
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

  )";

const std::string RapidSiliconYosysSurelogScript = R"( 
# Yosys/Surelog synthesis script for ${TOP_MODULE}
# Read source files
plugin -i systemverilog
read_systemverilog -sv -v ${PRIMITIVES_BLACKBOX}
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

  )";

const std::string RapidSiliconYosysGhdlScript = R"( 
# Yosys/Ghdl synthesis script for ${TOP_MODULE}
# Read source files
plugin -i ghdl
read_verilog -sv ${PRIMITIVES_BLACKBOX}
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

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
      if (m_mapToTechnology.empty()) m_mapToTechnology = "genesis3";
      if (m_yosysPluginLib.empty()) m_yosysPluginLib = "synth-rs";
      if (m_yosysPlugin.empty()) m_yosysPlugin = "synth_rs";
      switch (GetParserType()) {
        case ParserType::Verific: {
          YosysScript(RapidSiliconYosysVerificScript);
          break;
        }
        case ParserType::Surelog: {
          YosysScript(RapidSiliconYosysSurelogScript);
          break;
        }
        case ParserType::GHDL: {
          YosysScript(RapidSiliconYosysGhdlScript);
          break;
        }
        case ParserType::Default: {
          YosysScript(RapidSiliconYosysDefaultScript);
          break;
        }
      }
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
  std::string io_inference;
  switch (m_synthIO) {
    case SynthesisIOInference::Auto:
      io_inference = "";
      break;
    case SynthesisIOInference::None:
      io_inference = "-no_iobuf";
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
  std::string no_flatten_mode;
  if (m_synthNoFlatten) {
    no_flatten_mode = "-no_flatten";
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
  limits += std::string("-max_lut ") + std::to_string(MaxDeviceLUTCount()) +
            std::string(" ");
  limits += std::string("-max_reg ") + std::to_string(MaxDeviceFFCount()) +
            std::string(" ");
  limits += std::string("-max_device_dsp ") +
            std::to_string(MaxDeviceDSPCount()) + std::string(" ");
  limits += std::string("-max_device_bram ") +
            std::to_string(MaxDeviceBRAMCount()) + std::string(" ");
  limits += std::string("-max_device_carry_length ") +
            std::to_string(MaxDeviceCarryLength()) + std::string(" ");
  if (MaxUserDSPCount() >= 0)
    limits += std::string("-max_dsp ") + std::to_string(MaxUserDSPCount()) +
              std::string(" ");
  if (MaxUserBRAMCount() >= 0)
    limits += std::string("-max_bram ") + std::to_string(MaxUserBRAMCount()) +
              std::string(" ");
  if (MaxUserCarryLength() >= 0)
    limits += std::string("-max_carry_length ") +
              std::to_string(MaxUserCarryLength()) + std::string(" ");

  if (m_synthType == SynthesisType::QL) {
    optimization = "";
    effort = "";
    fsm_encoding = "";
    fast_mode = "";
    no_flatten_mode = "";
    carry_inference = "";
    io_inference = "";
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
  result = ReplaceAll(result, "${IO}", io_inference);
  result = ReplaceAll(result, "${FAST}", fast_mode);
  result = ReplaceAll(result, "${NO_FLATTEN}", no_flatten_mode);
  result = ReplaceAll(result, "${MAX_THREADS}", max_threads);
  result = ReplaceAll(result, "${NO_SIMPLIFY}", no_simplify);
  result = ReplaceAll(result, "${CLKE_STRATEGY}", clke_strategy);
  result = ReplaceAll(result, "${CEC}", cec);
  result = ReplaceAll(result, "${PLUGIN_LIB}", YosysPluginLibName());
  result = ReplaceAll(result, "${PLUGIN_NAME}", YosysPluginName());
  result = ReplaceAll(result, "${MAP_TO_TECHNOLOGY}", YosysMapTechnology());

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
  m_name = "CompilerRS";
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

        // TODO, model is invalid:
        // GetSimulator()->AddGateSimulationModel(tech_datapath /
        // "RS_PRIMITIVES" / "CARRY_CHAIN" / "CARRY_CHAIN.v");

        // TODO, only one version of BRAM, DSP FIFO need to stay

        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "DSP38" / "DSP38.v");

        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "TDP_RAM36K" / "TDP_RAM36K.v");

        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "FIFO" / "FIFO18K.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "FIFO" / "FIFO36K.v");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "FIFO" / "FIFO.v");

        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "LUT" / "LUT.v");

        // TODO: model is not valid for IVerilog/Verilator
        // GetSimulator()->AddGateSimulationModel(tech_datapath /
        // "RS_PRIMITIVES" / "IO" / "IO_MODELS" / "PLL" / "PLL_ADVANCE.sv");

#ifdef PRODUCTION_BUILD
        // TODO: We cannot ship the GB models, need abstract models
#else
        GetSimulator()->AddGateSimulationModel(
            tech_datapath / "RS_PRIMITIVES" / "IO" / "IO_MODELS" /
            "GBX_CONFIG" / "GEARBOX_CONFIG_PKG.sv");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "IO" / "io_cells_primitives.sv");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "IO" / "io_cells_sims.sv");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "IO" / "IO_MODELS" / "GBX" /
                                               "gbox_top.sv");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "IO" / "IO_MODELS" / "GBX" /
                                               "delay_line_tap64.sv");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "IO" / "IO_MODELS" / "GBX" /
                                               "phase_sel.sv");
        GetSimulator()->AddGateSimulationModel(tech_datapath / "RS_PRIMITIVES" /
                                               "IO" / "IO_MODELS" / "GBX" /
                                               "gbox_bslip.sv");
        ProjManager()->addLibraryPath(
            (tech_datapath / "RS_PRIMITIVES" / "IO" / "IO_MODELS" / "GBX")
                .string());
        ProjManager()->addLibraryExtension(".sv");
#endif

        if (FileUtils::FileExists(tech_datapath / "llatches_sim.v")) {
          GetSimulator()->AddGateSimulationModel(tech_datapath /
                                                 "llatches_sim.v");
        }
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

std::vector<std::string> CompilerRS::helpTags() const {
#ifdef PRODUCTION_BUILD
  return {"production"};
#else
  return {"engineering"};
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
      if (option == "-inferred_io") {
        compiler->SynthIO(SynthesisIOInference::Auto);
        continue;
      } else if (option == "-no_inferred_io") {
        compiler->SynthIO(SynthesisIOInference::None);
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
      if (option == "-carry_chain_limit" && i + 1 < argc) {
        std::string arg = argv[++i];
        const auto &[value, ok] = StringUtils::to_number<uint32_t>(arg);
        if (ok) compiler->MaxUserCarryLength(value);
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
      if (option == "-no_flatten") {
        compiler->SynthNoFlatten(true);
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
    if (compiler->GuiTclSync()) compiler->GuiTclSync()->saveSettings();
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
  bool status =
      cfgcompiler->RegisterCallbackFunction("assembler", BitAssembler_entry);
  return status;
}

std::string CompilerRS::BaseVprCommand(BaseVprDefaults defaults) {
  std::string device_size = "";
  if (PackOpt() == Compiler::PackingOpt::Debug) {
    device_size = " --device auto";
  } else if (!m_deviceSize.empty()) {
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
  netlistFile = FilePath(Action::Synthesis, netlistFile).string();
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
  if (ClbPackingOption() == ClbPacking::Timing_driven) {
    pnrOptions += " --allow_unrelated_clustering off";
  } else {
    pnrOptions += " --allow_unrelated_clustering on";
  }
  if (!PnROpt().empty()) pnrOptions += " " + PnROpt();
  if (!PerDevicePnROptions().empty()) pnrOptions += " " + PerDevicePnROptions();
  if (defaults.gen_post_synthesis_netlist)
    if (pnrOptions.find("gen_post_synthesis_netlist") == std::string::npos) {
      pnrOptions += " --gen_post_synthesis_netlist on";
    }
  if (pnrOptions.find("post_synth_netlist_unconn_inputs") ==
      std::string::npos) {
    pnrOptions += " --post_synth_netlist_unconn_inputs gnd";
  }
  if (pnrOptions.find("inner_loop_recompute_divider") == std::string::npos) {
    pnrOptions += " --inner_loop_recompute_divider 1";
  }
  if (pnrOptions.find("max_router_iterations") == std::string::npos) {
    pnrOptions += " --max_router_iterations 1500";
  }
  if (pnrOptions.find("timing_report_detail") == std::string::npos) {
    pnrOptions += " --timing_report_detail detailed";
  }
  if (pnrOptions.find("timing_report_npaths") == std::string::npos) {
    pnrOptions += " --timing_report_npaths 100";
  }
  std::string vpr_skip_fixup;
  if (m_pb_pin_fixup == "pb_pin_fixup") {
    // Skip
    vpr_skip_fixup = "on";
  } else {
    // Don't skip
    vpr_skip_fixup = "off";
  }

  auto sdcFile =
      FilePath(Action::Pack, ProjManager()->projectName() + "_openfpga.sdc")
          .string();
  std::string command =
      m_vprExecutablePath.string() + std::string(" ") +
      m_architectureFile.string() + std::string(" ") +
      std::string(
          netlistFile + std::string(" --sdc_file ") + sdcFile +
          std::string(" --route_chan_width ") +
          std::to_string(m_channel_width) +
          " --suppress_warnings check_rr_node_warnings.log,check_rr_node"
          " --clock_modeling ideal --absorb_buffer_luts off "
          "--skip_sync_clustering_and_routing_results " +
          vpr_skip_fixup +
          " --constant_net_method route --post_place_timing_report " +
          m_projManager->projectName() + "_post_place_timing.rpt" +
          device_size + pnrOptions);
  if (!ProjManager()->DesignTopModule().empty())
    command += " --top " + ProjManager()->DesignTopModule();

  fs::path netlistFileName{netlistFile};
  netlistFileName = netlistFileName.filename();
  auto name = netlistFileName.stem().string();
  command += " --net_file " + FilePath(Action::Pack, name + ".net").string();
  command +=
      " --place_file " + FilePath(Action::Detailed, name + ".place").string();
  command +=
      " --route_file " + FilePath(Action::Routing, name + ".route").string();
  return command;
}

void CompilerRS::Version(std::ostream *out) {
  (*out) << "Rapid Silicon Raptor Design Suite"
         << "\n";
  LogUtils::PrintVersion(out);
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
        "${OPTIMIZATION} ${EFFORT} ${FSM_ENCODING} ${CARRY} ${FAST} "
        "${NO_FLATTEN}");

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
    auto carry = StringUtils::to_string(compiler->MaxUserCarryLength());
    tclOptions += " -carry_chain_limit " + carry;
  };
  return tclOptions;
}

// This takes an arglist generated by the settings dialog and applies the
// options to CompilerRs
void FOEDAG::TclArgs_setRsSynthesisOptions(const std::string &argsStr) {
  CompilerRS *compiler = (CompilerRS *)GlobalSession->GetCompiler();

  bool fast{false};
  bool no_flatten{false};
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
    if (option == "-carry_chain_limit" && tokens.size() > 1) {
      std::string arg = tokens[1];
      const auto &[value, ok] = StringUtils::to_number<uint32_t>(arg);
      if (ok) compiler->MaxUserCarryLength(value);
      continue;
    }
    if (option == "-fast") {
      fast = true;
      continue;
    }
    if (option == "-no_flatten") {
      no_flatten = true;
      continue;
    }
  }
  compiler->SynthFast(fast);
  compiler->SynthNoFlatten(no_flatten);
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
  const std::string openStaFile = ProjManager()->projectName() + "_opensta.tcl";
  FileUtils::WriteToFile(openStaFile, script);
  return openStaFile;
}

bool CompilerRS::TimingAnalysis() {
  auto guard =
      sg::make_scope_guard([this] { RenamePostSynthesisFiles(Action::STA); });
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

  auto workingDir = FilePath(Action::STA).string();
  if (TimingAnalysisOpt() == STAOpt::View) {
    TimingAnalysisOpt(STAOpt::None);
    const std::string command =
        BaseVprCommand({false}) + " --analysis --disp on";
    const int status =
        ExecuteAndMonitorSystemCommand(command, {}, false, workingDir);
    if (status) {
      ErrorMessage("Design " + ProjManager()->projectName() +
                   " place and route view failed!");
      return false;
    }
    return true;
  }

  fs::path netlistPath = GetNetlistPath();
  auto netlistFileName = netlistPath.filename().stem().string();
  if (FileUtils::IsUptoDate(
          FilePath(Action::Routing, netlistFileName + ".route").string(),
          FilePath(Action::STA, "timing_analysis.rpt").string())) {
    Message("Design " + ProjManager()->projectName() + " timing didn't change");
    return true;
  }
  int status = 0;
  std::string taCommand;
  // use OpenSTA to do the job
  auto file = ProjManager()->projectName() + "_sta.cmd";
  if (TimingAnalysisEngineOpt() == STAEngineOpt::Opensta) {
    // allows SDF to be generated for OpenSTA

    // calls stars to generate files for opensta
    // stars pass all arguments to vpr to generate design context
    // then it does it work based on that
    std::string vpr_executable_path =
        m_vprExecutablePath.string() + std::string(" ");
    std::string command = BaseVprCommand({false});
    if (command.find(vpr_executable_path) != std::string::npos) {
      command = ReplaceAll(command, vpr_executable_path,
                           m_starsExecutablePath.string() + " ");
    }
    if (!FileUtils::FileExists(m_starsExecutablePath)) {
      ErrorMessage("Cannot find executable: " + m_starsExecutablePath.string());
      return false;
    }
    FileUtils::WriteToFile(file, command);
    int status = ExecuteAndMonitorSystemCommand(command, {}, false, workingDir);
    if (status) {
      ErrorMessage("Design " + ProjManager()->projectName() +
                   " timing analysis failed!");
      return false;
    }
    // find files
    auto topModule = ProjManager()->getDesignTopModule().toStdString();
    if (topModule.empty()) {
      auto topModules = TopModules(FilePath(Action::Analyze, "port_info.json"));
      for (const std::string &top : topModules) {
        if (FileUtils::FileExists(FilePath(Action::STA, top + "_stars.lib"))) {
          topModule = top;
          break;
        }
      }
    }
    if (topModule.empty()) {
      ErrorMessage(
          "Can't find top module or file *_stars.lib is not generated");
      return false;
    }
    auto libFileName = FilePath(Action::STA, topModule + "_stars.lib");
    auto netlistFileName = FilePath(Action::STA, topModule + "_stars.v");
    auto sdfFileName = FilePath(Action::STA, topModule + "_stars.sdf");
    auto sdcFileName = FilePath(Action::STA, topModule + "_stars.sdc");
    if (std::filesystem::is_regular_file(libFileName) &&
        std::filesystem::is_regular_file(netlistFileName) &&
        std::filesystem::is_regular_file(sdfFileName) &&
        std::filesystem::is_regular_file(sdcFileName)) {
      taCommand = BaseStaCommand() + " " +
                  BaseStaScript(libFileName.string(), netlistFileName.string(),
                                sdfFileName.string(), sdcFileName.string());
      FileUtils::WriteToFile(file, taCommand);
    } else {
      auto fileList =
          StringUtils::join({libFileName.string(), netlistFileName.string(),
                             sdfFileName.string(), sdcFileName.string()},
                            "\n");
      ErrorMessage(
          "No required design info generated for user design, required for "
          "timing analysis:\n" +
          fileList);
      return false;
    }
  } else {  // use vpr/tatum engine
    taCommand = BaseVprCommand({false}) + " --analysis";
    FileUtils::WriteToFile(file, taCommand + " --disp on");
  }

  status = ExecuteAndMonitorSystemCommand(taCommand, TIMING_ANALYSIS_LOG, false,
                                          workingDir);
  RenamePostSynthesisFiles(Action::STA);
  if (status) {
    ErrorMessage("Design " + ProjManager()->projectName() +
                 " timing analysis failed!");
    return false;
  }

  Message("Design " + ProjManager()->projectName() + " is timing analysed!");

  return true;
}

bool CompilerRS::PowerAnalysis() {
  if (!ProjManager()->HasDesign()) {
    ErrorMessage("No design specified");
    return false;
  }
  if (!HasTargetDevice()) return false;

  if (PowerAnalysisOpt() == PowerOpt::Clean) {
    Message("Cleaning PowerAnalysis results for " +
            ProjManager()->projectName());
    PowerAnalysisOpt(PowerOpt::None);
    // m_state = State::PowerAnalyzed;
    CleanFiles(Action::Power);
    return true;
  }

  PERF_LOG("PowerAnalysis has started");
  Message("##################################################");
  Message("Power Analysis for design: " + ProjManager()->projectName());
  Message("##################################################");

  std::string netlistFile;
  switch (GetNetlistType()) {
    case NetlistType::Verilog:
      netlistFile = ProjManager()->projectName() + "_post_synth.v";
      break;
    case NetlistType::VHDL:
      // Until we have a VHDL netlist reader in VPR
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
  netlistFile = FilePath(Action::Synthesis, netlistFile).string();

  for (const auto &lang_file : ProjManager()->DesignFiles()) {
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

  if (FileUtils::IsUptoDate(netlistFile,
                            FilePath(Action::Power, "power.csv").string())) {
    Message("Design " + ProjManager()->projectName() +
            " power didn't change, skipping power analysis.");
    return true;
  }

  std::string sdcFile;
  const auto &constrFiles = ProjManager()->getConstrFiles();
  for (const auto &file : constrFiles) {
    if (file.find(".sdc") != std::string::npos) {
      sdcFile = file;
      break;
    }
  }

  std::filesystem::path binpath = GetSession()->Context()->BinaryPath();
  std::filesystem::path tclLibPath = binpath / ".." / "lib";
  std::filesystem::path tclLibraryPath = binpath / ".." / "lib" / "tcl8.6";
  SetEnvironmentVariable("TCLLIBPATH", tclLibPath.string());
  SetEnvironmentVariable("TCL_LIBRARY", tclLibraryPath.string());
  std::string command = m_tclExecutablePath.string() + " ";
  command += m_powerExecutablePath.string() + " ";
  command += "--netlist=" + netlistFile + " ";
  if (!sdcFile.empty()) command += "--sdc=" + sdcFile + " ";

  // Use the following sub job to run the power tcl script: raptor --cmd "cmd"
  // --script <script>
  /* This code fails in CI on CentOS so we use the tclsh approach above:
  std::string command = m_raptorExecutablePath.string() + " ";
  command += "--cmd \"";
  command += "set netlist_file " + netlistFile + ";";
  if (!sdcFile.empty()) {
    command += "set sdc " + sdcFile + ";";
  }
  command += "\" ";
  command += "--batch ";
  command += "--script " + m_powerExecutablePath.string() + " ";
  */

  auto file = ProjManager()->projectName() + "_power.cmd";
  FileUtils::WriteToFile(file, command);

  int status = ExecuteAndMonitorSystemCommand(
      command, POWER_ANALYSIS_LOG, false, FilePath(Action::Power).string());
  if (status) {
    ErrorMessage("Design " + ProjManager()->projectName() +
                 " power analysis failed");
    return false;
  }

  Message("Design " + ProjManager()->projectName() + " is power analysed");
  return true;
}
