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

#include "CFGCommonRS/CFGArgRS_auto.h"
#include "Compiler/CompilerRS.h"
#include "Compiler/Constraints.h"
#include "Compiler/Log.h"
#include "Configuration/CFGCompiler/CFGCompiler.h"
#include "ConfigurationRS/BitAssembler/BitAssembler.h"
#include "ConfigurationRS/Ocla/Ocla.h"
#include "Main/Settings.h"
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

${PLUGIN_NAME} -post_cleanup 1 -legalize_ram_clk_ports ${NEW_IO_BUF_MAP} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${KEEP_TRIBUF} ${NEW_DSP19X2} ${NEW_TDP36K} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

plugin -i design-edit
design_edit -tech ${MAP_TO_TECHNOLOGY} -sdc ${PIN_LOCATION_SDC} -json ${CONFIG_JSON} -w ${OUTPUT_WRAPPER_VERILOG} ${OUTPUT_WRAPPER_EBLIF} -pr post_pnr_${OUTPUT_WRAPPER_VERILOG} post_pnr_${OUTPUT_WRAPPER_EBLIF}
${OUTPUT_FABRIC_NETLIST}

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

${PLUGIN_NAME} -post_cleanup 1 -legalize_ram_clk_ports ${NEW_IO_BUF_MAP} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${KEEP_TRIBUF} ${NEW_DSP19X2} ${NEW_TDP36K} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

plugin -i design-edit
design_edit -tech ${MAP_TO_TECHNOLOGY} -sdc ${PIN_LOCATION_SDC} -json ${CONFIG_JSON} -w ${OUTPUT_WRAPPER_VERILOG} ${OUTPUT_WRAPPER_EBLIF} -pr post_pnr_${OUTPUT_WRAPPER_VERILOG} post_pnr_${OUTPUT_WRAPPER_EBLIF}
${OUTPUT_FABRIC_NETLIST}

  )";

const std::string RapidSiliconYosysGateLevelScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
read_verilog -sv ${PRIMITIVES_BLACKBOX}
${READ_DESIGN_FILES}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

${OUTPUT_NETLIST}

plugin -i design-edit
design_edit -tech ${MAP_TO_TECHNOLOGY} -sdc ${PIN_LOCATION_SDC} -json ${CONFIG_JSON} -w ${OUTPUT_WRAPPER_VERILOG} ${OUTPUT_WRAPPER_EBLIF} -pr post_pnr_${OUTPUT_WRAPPER_VERILOG} post_pnr_${OUTPUT_WRAPPER_EBLIF}
${OUTPUT_FABRIC_NETLIST}

  )";

const std::string RapidSiliconYosysSurelogScript = R"( 
# Yosys/Surelog synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}${PRIMITIVES_BLACKBOX}

# Technology mapping
hierarchy ${TOP_MODULE_DIRECTIVE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -post_cleanup 1 -legalize_ram_clk_ports ${NEW_IO_BUF_MAP} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${KEEP_TRIBUF} ${NEW_DSP19X2} ${NEW_TDP36K} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

plugin -i design-edit
design_edit -tech ${MAP_TO_TECHNOLOGY} -sdc ${PIN_LOCATION_SDC} -json ${CONFIG_JSON} -w ${OUTPUT_WRAPPER_VERILOG} ${OUTPUT_WRAPPER_EBLIF} -pr post_pnr_${OUTPUT_WRAPPER_VERILOG} post_pnr_${OUTPUT_WRAPPER_EBLIF}
${OUTPUT_FABRIC_NETLIST}

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

${PLUGIN_NAME} -post_cleanup 1 -legalize_ram_clk_ports ${NEW_IO_BUF_MAP} -tech ${MAP_TO_TECHNOLOGY} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${IO} ${KEEP_TRIBUF} ${NEW_DSP19X2} ${NEW_TDP36K} ${LIMITS} ${FSM_ENCODING} ${FAST} ${NO_FLATTEN} ${MAX_THREADS} ${NO_SIMPLIFY} ${CLKE_STRATEGY} ${CEC}

${OUTPUT_NETLIST}

plugin -i design-edit
design_edit -tech ${MAP_TO_TECHNOLOGY} -sdc ${PIN_LOCATION_SDC} -json ${CONFIG_JSON} -w ${OUTPUT_WRAPPER_VERILOG} ${OUTPUT_WRAPPER_EBLIF} -pr post_pnr_${OUTPUT_WRAPPER_VERILOG} post_pnr_${OUTPUT_WRAPPER_EBLIF}
${OUTPUT_FABRIC_NETLIST}

  )";

const std::string RapidSiliconYosysPowerExtractionScript =
    R"(plugin -i pow-extract
read_verilog ${VERILOG_FILE}
pow_extract -tech genesis3 ${SDC})";

static auto assembler_flow(CompilerRS *compiler, bool batchMode, int argc,
                           const char *argv[]) {
  CFGCompiler *cfgcompiler = compiler->GetConfiguration();
  compiler->BitsFlags(BitstreamFlags::DefaultBitsOpt);
  compiler->BitsOpt(Compiler::BitstreamOpt::None);
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "force") {
      compiler->BitsFlags(BitstreamFlags::Force);
    } else if (arg == "clean") {
      compiler->BitsOpt(Compiler::BitstreamOpt::Clean);
    } else if (arg == "enable_simulation") {
      compiler->BitsFlags(BitstreamFlags::EnableSimulation);
    } else {
      compiler->ErrorMessage("Unknown bitstream option: " + arg);
      return TCL_ERROR;
    }
  }

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

  // Bitstream flow might call other configuration command for example
  // model_config. Set the command last
  cfgcompiler->m_cmdarg.command = "assembler";
  cfgcompiler->m_cmdarg.clean =
      compiler->BitsOpt() == Compiler::BitstreamOpt::Clean;
  return TCL_OK;
}

static bool debugger_flow(CompilerRS *compiler, bool batchMode, int argc,
                          const char *argv[]) {
  CFGCompiler *cfgcompiler = compiler->GetConfiguration();
  cfgcompiler->m_cmdarg.command = "debugger";
  cfgcompiler->m_cmdarg.compilerName = compiler->Name();
  auto arg = std::make_shared<CFGArg_DEBUGGER>();
  // drop the first executable arg,
  // create pointer to the second element
  // the parser expect the first arg is the command name
  const char **argvPtr = &argv[1];
  std::vector<std::string> errors;
  bool status = arg->parse(argc - 1, argvPtr, &errors);
  cfgcompiler->m_cmdarg.arg = arg;
  cfgcompiler->m_cmdarg.toolPath = compiler->GetProgrammerToolExecPath();
  return status;
}

std::string CompilerRS::InitSynthesisScript() {
  for (const auto &lang_file : ProjManager()->DesignFiles()) {
    switch (lang_file.first.language) {
      case Design::Language::VERILOG_NETLIST:
        YosysScript(RapidSiliconYosysGateLevelScript);
        return m_yosysScript;
        break;
      default:
        break;
    }
  }

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
  std::filesystem::path latestPrimitivesBlackboxPath =
      tech_datapath / "FPGA_PRIMITIVES_MODELS" / "blackbox_models" /
      "cell_sim_blackbox.v";
  if (FileUtils::FileExists(latestPrimitivesBlackboxPath)) {
    primitivesBlackboxPath = latestPrimitivesBlackboxPath;
  }
  std::string result;
  if (FileUtils::FileExists(primitivesBlackboxPath)) {
    switch (GetParserType()) {
      case ParserType::Default:
        result = "read_verilog -sv " + primitivesBlackboxPath.string() + "\n";
        break;
      case ParserType::GHDL:
        result = "read_verilog -sv " + primitivesBlackboxPath.string() + "\n";
        break;
      case ParserType::Surelog:
        result = "read_verilog -sv " + primitivesBlackboxPath.string() + "\n";
        break;
      case ParserType::Verific:
        result = "-sv2009 " + primitivesBlackboxPath.string() + "\n";
        break;
    }
  }
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
  std::filesystem::path latestPrimitivesBlackboxPath =
      tech_datapath / "FPGA_PRIMITIVES_MODELS" / "blackbox_models" /
      "cell_sim_blackbox.v";
  if (FileUtils::FileExists(latestPrimitivesBlackboxPath)) {
    primitivesBlackboxPath = latestPrimitivesBlackboxPath;
  }
  if (!FileUtils::FileExists(primitivesBlackboxPath)) {
    primitivesBlackboxPath = "";
  }
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
  switch (SynthOptimization()) {
    case SynthesisOptimization::Area:
      optimization = "-de -goal area";
      break;
    case SynthesisOptimization::Delay:
      optimization = "-de -goal delay";
      break;
    case SynthesisOptimization::Mixed:
      optimization = "-de -goal mixed";
      break;
  }
  std::string io_inference;
  switch (m_synthIO) {
    case SynthesisIOInference::Auto:
      io_inference = "";
      if ((!BaseDeviceName().empty()) && (BaseDeviceName()[0] == 'e')) {
        // eFPGA don't typically have IOs, unless the user explicitly requested
        // it
        if (m_userSynthIOSetting == SynthesisIOInference::None) {
          io_inference = "-no_iobuf";
        }
      }
      break;
    case SynthesisIOInference::None:
      io_inference = "-no_iobuf";
      break;
  }
  std::string keep_tribuf{};
  if (KeepTribuf()) {
    // TODO: Synthesis does not write out IOs correctly yet in VHDL
    // Once that is fixed, this if statement can be removed
    if (GetNetlistType() != NetlistType::VHDL) {
      keep_tribuf = "-keep_tribuf";
    }
  }
  std::string new_dsp19x2{};
  if (NewDsp19x2()) new_dsp19x2 = "-new_dsp19x2";
  std::string new_tdp36k{};
  if (NewTdp36k()) new_tdp36k = "-new_tdp36k";
  std::string new_ioBufMap{};
  if (NewIOBufMap()) new_ioBufMap = "-new_iobuf_map 3 -iofab_map 1";
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
    keep_tribuf.clear();
    new_dsp19x2.clear();
    new_ioBufMap.clear();
    new_tdp36k.clear();
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
  result = ReplaceAll(result, "${KEEP_TRIBUF}", keep_tribuf);
  result = ReplaceAll(result, "${NEW_DSP19X2}", new_dsp19x2);
  result = ReplaceAll(result, "${NEW_IO_BUF_MAP}", new_ioBufMap);
  result = ReplaceAll(result, "${NEW_TDP36K}", new_tdp36k);
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
      // Temporary, once planner works with Verilog, only output Verilog
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_verilog -noexpr -nodec -norename -v "
                     "${OUTPUT_VERILOG}\nwrite_blif -param ${OUTPUT_EBLIF}");
      result = ReplaceAll(
          result, "${OUTPUT_FABRIC_NETLIST}",
          "write_verilog -noexpr -nodec -norename -v "
          "${OUTPUT_FABRIC_VERILOG}\nwrite_blif -param ${OUTPUT_FABRIC_EBLIF}");
      break;
    case NetlistType::VHDL:
      // Temporary, once planner and the Packer work with VHDL, replace by just
      // VHDL
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_vhdl ${OUTPUT_VHDL}\nwrite_verilog "
                     "${OUTPUT_VERILOG}\nwrite_blif -param ${OUTPUT_EBLIF}");
      result = ReplaceAll(
          result, "${OUTPUT_FABRIC_NETLIST}",
          "write_vhdl ${OUTPUT_FABRIC_VHDL}\nwrite_verilog "
          "${OUTPUT_FABRIC_VERILOG}\nwrite_blif -param ${OUTPUT_FABRIC_EBLIF}");
      break;
    case NetlistType::Edif:
      // Temporary, once planner works with Verilog, only output edif
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_edif ${OUTPUT_EDIF}\nwrite_blif ${OUTPUT_BLIF}");
      result = ReplaceAll(
          result, "${OUTPUT_FABRIC_NETLIST}",
          "write_edif ${OUTPUT_FABRIC_EDIF}\nwrite_blif ${OUTPUT_FABRIC_BLIF}");
      break;
    case NetlistType::Blif:
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_verilog -noexpr -nodec -norename -v "
                     "${OUTPUT_VERILOG}\nwrite_blif -param ${OUTPUT_BLIF}");
      result = ReplaceAll(
          result, "${OUTPUT_FABRIC_NETLIST}",
          "write_verilog -noexpr -nodec -norename -v "
          "${OUTPUT_FABRIC_VERILOG}\nwrite_blif -param ${OUTPUT_FABRIC_BLIF}");
      break;
    case NetlistType::EBlif:
      result =
          ReplaceAll(result, "${OUTPUT_NETLIST}",
                     "write_verilog -noexpr -nodec -norename -v "
                     "${OUTPUT_VERILOG}\nwrite_blif -param ${OUTPUT_EBLIF}");
      result = ReplaceAll(
          result, "${OUTPUT_FABRIC_NETLIST}",
          "write_verilog -noexpr -nodec -norename -v "
          "${OUTPUT_FABRIC_VERILOG}\nwrite_blif -param ${OUTPUT_FABRIC_EBLIF}");
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
    case NetlistType::Edif:
    case NetlistType::Blif:
    case NetlistType::EBlif:
      if (action == Simulator::SimulationType::Gate ||
          action == Simulator::SimulationType::PNR) {
        if (FileUtils::FileExists(tech_datapath / "simlib.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath / "simlib.v");
        if (FileUtils::FileExists(tech_datapath / "brams_sim.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath / "brams_sim.v");
        if (FileUtils::FileExists(tech_datapath / "TDP18K_FIFO.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath /
                                                 "TDP18K_FIFO.v");
        if (FileUtils::FileExists(tech_datapath / "ufifo_ctl.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath / "ufifo_ctl.v");
        if (FileUtils::FileExists(tech_datapath / "sram1024x18.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath /
                                                 "sram1024x18.v");
        if (FileUtils::FileExists(tech_datapath / "llatches_sim.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath /
                                                 "llatches_sim.v");
        if (FileUtils::FileExists(tech_datapath / "cells_sim.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath / "cells_sim.v");
        std::filesystem::path path =
            tech_datapath / "FPGA_PRIMITIVES_MODELS" / "sim_models" / "verilog";
        if (FileUtils::FileExists(path)) {
          for (const std::filesystem::path &entry :
               std::filesystem::directory_iterator(path)) {
            if (entry.filename() == "adder_carry.v") continue;
            GetSimulator()->AddGateSimulationModel(entry);
          }
        }
      }
      if (action == Simulator::SimulationType::PNR) {
        if (FileUtils::FileExists(tech_datapath / "primitives.v"))
          GetSimulator()->AddGateSimulationModel(tech_datapath /
                                                 "primitives.v");
        // Temporary solution: Post PnR reverse mapping RAM model
        std::filesystem::path reverse_map_tdp36 =
            tech_datapath / "FPGA_PRIMITIVES_MODELS" / "sim_models_internal" /
            "primitives_mapping" / "BRAM" / "rs_tdp36k_post_pnr_mapping.v";
        if (FileUtils::FileExists(reverse_map_tdp36))
          GetSimulator()->AddGateSimulationModel(reverse_map_tdp36);
        // Temporary solution: Post PnR reverse mapping DSP model
        std::filesystem::path reverse_map_dsp38 =
            tech_datapath / "FPGA_PRIMITIVES_MODELS" / "sim_models_internal" /
            "primitives_mapping" / "DSP" / "rs_dsp_multxxx_post_pnr_mapping.v";
        if (FileUtils::FileExists(reverse_map_dsp38))
          GetSimulator()->AddGateSimulationModel(reverse_map_dsp38);
      }
      break;
    case NetlistType::VHDL:
      if (FileUtils::FileExists(tech_datapath / "cells_sim.vhd"))
        GetSimulator()->AddGateSimulationModel(tech_datapath / "cells_sim.vhd");
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
      if (option == "-keep_tribuf") {
        compiler->KeepTribuf(true);
        continue;
      }
      if (option == "-no_tribuf") {
        compiler->KeepTribuf(false);
        continue;
      }
      if (option == "-new_dsp19x2") {
        compiler->NewDsp19x2(true);
        continue;
      }
      if (option == "-new_iobuf_map") {
        compiler->NewIOBufMap(true);
        continue;
      }
      if (option == "-no_iobuf_map") {
        compiler->NewIOBufMap(false);
        continue;
      }
      if (option == "-no_dsp19x2") {
        compiler->NewDsp19x2(false);
        continue;
      }
      if (option == "-new_tdp36k") {
        compiler->NewTdp36k(true);
        continue;
      }
      if (option == "-no_tdp36k") {
        compiler->NewTdp36k(false);
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
      if ((status = assembler_flow(compiler, true, argc, argv)) == TCL_OK) {
        return CFGCompiler::Compile(cfgcompiler, true);
      } else {
        return status;
      }
    };
    interp->registerCmd("assembler", assembler, this, 0);

    auto debugger = [](void *clientData, Tcl_Interp *interp, int argc,
                       const char *argv[]) -> int {
      CompilerRS *compiler = (CompilerRS *)clientData;
      CFGCompiler *cfgcompiler = compiler->GetConfiguration();
      if (debugger_flow(compiler, true, argc, argv)) {
        int result = CFGCompiler::Compile(cfgcompiler, true);
        if (result == TCL_OK && !cfgcompiler->m_cmdarg.tclOutput.empty()) {
          cfgcompiler->GetCompiler()->TclInterp()->setResult(
              cfgcompiler->m_cmdarg.tclOutput);
          cfgcompiler->m_cmdarg.tclOutput.clear();
        }
        if ((result != TCL_OK) || (cfgcompiler->m_cmdarg.tclStatus != TCL_OK))
          return TCL_ERROR;
        return TCL_OK;
      } else {
        return TCL_ERROR;
      }
    };
    interp->registerCmd("debugger", debugger, this, 0);

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

    auto debugger = [](void *clientData, Tcl_Interp *interp, int argc,
                       const char *argv[]) -> int {
      CompilerRS *compiler = (CompilerRS *)clientData;
      CFGCompiler *cfgcompiler = compiler->GetConfiguration();
      if (debugger_flow(compiler, false, argc, argv)) {
        int result = CFGCompiler::Compile(cfgcompiler, false);
        if (result == TCL_OK && !cfgcompiler->m_cmdarg.tclOutput.empty()) {
          cfgcompiler->GetCompiler()->TclInterp()->setResult(
              cfgcompiler->m_cmdarg.tclOutput);
          cfgcompiler->m_cmdarg.tclOutput.clear();
        }
        if ((result != TCL_OK) || (cfgcompiler->m_cmdarg.tclStatus != TCL_OK))
          return TCL_ERROR;
        return TCL_OK;
      } else {
        return TCL_ERROR;
      }
    };
    interp->registerCmd("debugger", debugger, this, 0);
  }
  bool status =
      cfgcompiler->RegisterCallbackFunction("assembler", BitAssembler_entry);
  status &= cfgcompiler->RegisterCallbackFunction("debugger", Ocla_entry);
  return status;
}

std::string CompilerRS::BaseVprCommand(BaseVprDefaults defaults) {
  std::string device_size = "";
  if (PackOpt() == Compiler::PackingOpt::Debug) {
    device_size = " --device auto";
  } else if (!m_deviceSize.empty()) {
    device_size = " --device " + m_deviceSize;
  }
  const std::string netlistFile = GetNetlistPath();
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
      FilePath(Action::Pack,
               "fabric_" + ProjManager()->projectName() + "_openfpga.sdc")
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
  if (m_flatRouting) {
    command += " --flat_routing true";
  }
  if (!m_routingGraphFile.empty()) {
    command += " --read_rr_graph " + m_routingGraphFile.string();
  }
  command += " --net_file " + FilePath(Action::Pack, name + ".net").string();
  command +=
      " --place_file " + FilePath(Action::Placement, name + ".place").string();
  command +=
      " --route_file " + FilePath(Action::Routing, name + ".route").string();
  processCustomLayout();
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
    License_Manager license(deviceName);

  } catch (License_Manager::LicenseFatalException const &ex) {
    return false;

  } catch (License_Manager::LicenseCorrectableException const &ex) {
    return false;
  }
  return true;

#endif
  return true;
}

// This collects the current compiler values and returns a string of args that
// will be used by the settings UI to populate its fields
ArgumentsMap FOEDAG::TclArgs_getRsSynthesisOptions() {
  CompilerRS *compiler = (CompilerRS *)GlobalSession->GetCompiler();
  ArgumentsMap argumets{};
  if (compiler) {
    // Use the script completer to generate an arg list w/ current values
    // Note we are only grabbing the values that matter for the settings ui
    std::string tclOptions = compiler->FinishSynthesisScript(
        "${OPTIMIZATION} ${EFFORT} ${FSM_ENCODING} ${CARRY}");

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

    argumets = parseArguments(tclOptions);
    argumets.addArgument("keep_tribuf",
                         compiler->KeepTribuf() ? "true" : "false");
    argumets.addArgument("no_flatten",
                         compiler->SynthNoFlatten() ? "true" : "false");
    argumets.addArgument("fast", compiler->SynthFast() ? "true" : "false");

    switch (compiler->GetNetlistType()) {
      case Compiler::NetlistType::Blif:
        argumets.addArgument("netlist_lang", "blif");
        break;
      case Compiler::NetlistType::EBlif:
        argumets.addArgument("netlist_lang", "eblif");
        break;
      case Compiler::NetlistType::Edif:
        argumets.addArgument("netlist_lang", "edif");
        break;
      case Compiler::NetlistType::VHDL:
        argumets.addArgument("netlist_lang", "vhdl");
        break;
      case Compiler::NetlistType::Verilog:
        argumets.addArgument("netlist_lang", "verilog");
        break;
    }

    if (compiler->SynthEffort() == CompilerRS::SynthesisEffort::None) {
      argumets.addArgument("effort", "none");
    }
    if (compiler->SynthFsm() == CompilerRS::SynthesisFsmEncoding::None) {
      argumets.addArgument("fsm_encoding", "none");
    }
    auto dsp = StringUtils::to_string(compiler->MaxUserDSPCount());
    argumets.addArgument("dsp_limit", dsp);
    auto bram = StringUtils::to_string(compiler->MaxUserBRAMCount());
    argumets.addArgument("bram_limit", bram);
    auto carry = StringUtils::to_string(compiler->MaxUserCarryLength());
    argumets.addArgument("carry_chain_limit", carry);

    if (!compiler->BaseDeviceName().empty() &&
        compiler->BaseDeviceName()[0] == 'e') {
      if (compiler->SynthIOUser() == CompilerRS::SynthesisIOInference::None) {
        argumets.addArgument("inferred_io", "none");
      } else if (compiler->SynthIOUser() ==
                 CompilerRS::SynthesisIOInference::Auto) {
        argumets.addArgument("inferred_io", "auto");
      }
    } else {
      if (compiler->SynthIO() == CompilerRS::SynthesisIOInference::None) {
        argumets.addArgument("inferred_io", "none");
      } else if (compiler->SynthIO() ==
                 CompilerRS::SynthesisIOInference::Auto) {
        argumets.addArgument("inferred_io", "auto");
      }
    }
  };
  return argumets;
}

// This takes an arglist generated by the settings dialog and applies the
// options to CompilerRs
void FOEDAG::TclArgs_setRsSynthesisOptions(const ArgumentsMap &argsStr) {
  CompilerRS *compiler = (CompilerRS *)GlobalSession->GetCompiler();
  if (!compiler) return;

  auto synthOpt = argsStr.value("_SynthOpt_");
  if (synthOpt) {
    if (synthOpt == "area") {
      compiler->SynthOptimization(SynthesisOptimization::Area);
    } else if (synthOpt == "delay") {
      compiler->SynthOptimization(SynthesisOptimization::Delay);
    } else if (synthOpt == "mixed") {
      compiler->SynthOptimization(SynthesisOptimization::Mixed);
    }
  }
  const auto &[fsmExist, fsm_encoding] = argsStr.value("fsm_encoding");
  if (fsmExist) {
    if (fsm_encoding == "binary") {
      compiler->SynthFsm(CompilerRS::SynthesisFsmEncoding::Binary);
    } else if (fsm_encoding == "onehot") {
      compiler->SynthFsm(CompilerRS::SynthesisFsmEncoding::Onehot);
    } else if (fsm_encoding == "none") {
      compiler->SynthFsm(CompilerRS::SynthesisFsmEncoding::None);
    }
  }

  auto effort = argsStr.value("effort");
  if (effort) {
    if (effort == "high") {
      compiler->SynthEffort(CompilerRS::SynthesisEffort::High);
    } else if (effort == "medium") {
      compiler->SynthEffort(CompilerRS::SynthesisEffort::Medium);
    } else if (effort == "low") {
      compiler->SynthEffort(CompilerRS::SynthesisEffort::Low);
    } else if (effort == "none") {
      compiler->SynthEffort(CompilerRS::SynthesisEffort::None);
    }
  }

  auto netlist_lang = argsStr.value("netlist_lang");
  if (netlist_lang) {
    if (netlist_lang == "blif") {
      compiler->SetNetlistType(Compiler::NetlistType::Blif);
    } else if (netlist_lang == "eblif") {
      compiler->SetNetlistType(Compiler::NetlistType::EBlif);
    } else if (netlist_lang == "edif") {
      compiler->SetNetlistType(Compiler::NetlistType::Edif);
    } else if (netlist_lang == "vhdl") {
      compiler->SetNetlistType(Compiler::NetlistType::VHDL);
    } else if (netlist_lang == "verilog") {
      compiler->SetNetlistType(Compiler::NetlistType::Verilog);
    }
  }

  auto carry = argsStr.value("carry");
  if (carry) {
    if (carry == "none") {
      compiler->SynthCarry(CompilerRS::SynthesisCarryInference::NoCarry);
    } else if (carry == "auto") {
      compiler->SynthCarry(CompilerRS::SynthesisCarryInference::Auto);
    } else if (carry == "all") {
      compiler->SynthCarry(CompilerRS::SynthesisCarryInference::All);
    } else if (carry == "<unset>") {
      compiler->SynthCarry(CompilerRS::SynthesisCarryInference::None);
    }
  }
  auto dsp_limit = argsStr.value("dsp_limit");
  if (dsp_limit) {
    const auto &[value, ok] = StringUtils::to_number<uint32_t>(dsp_limit);
    if (ok) compiler->MaxUserDSPCount(value);
  }
  auto bram_limit = argsStr.value("bram_limit");
  if (bram_limit) {
    const auto &[value, ok] = StringUtils::to_number<uint32_t>(bram_limit);
    if (ok) compiler->MaxUserBRAMCount(value);
  }
  auto carry_chain_limit = argsStr.value("carry_chain_limit");
  if (carry_chain_limit) {
    const auto &[value, ok] =
        StringUtils::to_number<uint32_t>(carry_chain_limit);
    if (ok) compiler->MaxUserCarryLength(value);
  }

  auto fast = argsStr.value("fast");
  if (fast) {
    // enable when '-fast true', or '-fast'
    compiler->SynthFast(fast == "true" || fast.value.empty());
  }
  auto no_flatten = argsStr.value("no_flatten");
  if (no_flatten) {
    // enable when '-no_flatten true', or '-no_flatten'
    compiler->SynthNoFlatten(no_flatten == "true" || no_flatten.value.empty());
  }
  auto keep_tribuf = argsStr.value("keep_tribuf");
  if (keep_tribuf) {
    // enable when '-keep_tribuf true', or '-keep_tribuf'
    compiler->KeepTribuf(keep_tribuf == "true" || keep_tribuf.value.empty());
  }
  auto io_buf = argsStr.value("inferred_io");
  if (io_buf) {
    if (io_buf == "none")
      compiler->SynthIO(CompilerRS::SynthesisIOInference::None);
    else if (io_buf == "auto")
      compiler->SynthIO(CompilerRS::SynthesisIOInference::Auto);
  }
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
#ifdef ENABLE_OPENSTA
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
#else
    Message(
        "Raptor was not compiled with OpenSTA enabled, defaulting to VPR's "
        "Tatum timing engine");
    taCommand = BaseVprCommand({false}) + " --analysis";
    FileUtils::WriteToFile(file, taCommand + " --disp on");
#endif

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
      netlistFile = ProjManager()->projectName() + "_post_synth.v";
      break;
    case NetlistType::Blif:
      netlistFile = ProjManager()->projectName() + "_post_synth.v";
      break;
    case NetlistType::EBlif:
      netlistFile = ProjManager()->projectName() + "_post_synth.v";
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

  std::string powerFile{"power.csv"};
  if (FileUtils::IsUptoDate(netlistFile,
                            FilePath(Action::Power, powerFile).string())) {
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
  if (!sdcFile.empty() &&
      FileUtils::IsUptoDate(sdcFile,
                            FilePath(Action::Power, powerFile).string())) {
    Message("Design " + ProjManager()->projectName() +
            " power didn't change, skipping power analysis.");
    return true;
  }

  std::string scriptPath = "pw_extract.ys";

  std::string pw_extractScript = RapidSiliconYosysPowerExtractionScript;
  pw_extractScript =
      ReplaceAll(pw_extractScript, "${VERILOG_FILE}", netlistFile);
  pw_extractScript =
      ReplaceAll(pw_extractScript, "${SDC}",
                 sdcFile.empty() ? std::string{} : "-sdc " + sdcFile);

  FileUtils::WriteToFile(scriptPath, pw_extractScript);

  std::string command = m_yosysExecutablePath.string() + " -s " + scriptPath +
                        " -l " + ProjManager()->projectName() + "_power.log";

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

void CompilerRS::adjustTargetDeviceDefaults() {
  FOEDAG::Settings *settings = GlobalSession->GetSettings();
  try {
    auto &synth = settings->getJson()["Tasks"]["Synthesis"];
    if (!BaseDeviceName().empty() && (BaseDeviceName()[0] == 'e')) {  // eFPGA
      synth["inferred_io"]["default"] = "None";
    } else {
      synth["inferred_io"]["default"] = "Auto";
    }
  } catch (std::exception &e) {
    ErrorMessage(e.what());
  }
}
