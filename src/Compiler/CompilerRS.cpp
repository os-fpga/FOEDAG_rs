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

using namespace FOEDAG;

const std::string QLYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}

# Technology mapping
hierarchy -top ${TOP_MODULE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}

${PLUGIN_NAME} -family ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${NO_DSP} ${NO_BRAM} ${FSM_ENCODING} -blif ${OUTPUT_BLIF}

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
${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION} ${EFFORT} ${CARRY} ${NO_DSP} ${NO_BRAM} ${FSM_ENCODING}

# Clean and output blif
write_blif ${OUTPUT_BLIF}
write_verilog -noexpr -nodec -defparam -norename ${OUTPUT_VERILOG}
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
      optimization = "";
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
    case SynthesisCarryInference::NoConst:
      carry_inference = "-carry no_const";
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
  if (m_synthType == SynthesisType::QL) {
    optimization = "";
    effort = "";
    fsm_encoding = "";
    carry_inference = "";
    if (m_synthNoAdder) {
      optimization += " -no_adder";
    }
  }
  optimization += " " + DefaultSynthOptions();
  optimization += " " + SynthMoreOpt();
  result = ReplaceAll(result, "${OPTIMIZATION}", optimization);
  result = ReplaceAll(result, "${EFFORT}", effort);
  result = ReplaceAll(result, "${FSM_ENCODING}", fsm_encoding);
  result = ReplaceAll(result, "${CARRY}", carry_inference);
  result = ReplaceAll(result, "${NO_DSP}", no_dsp);
  result = ReplaceAll(result, "${NO_BRAM}", no_bram);
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
        } else if (arg == "no_const") {
          compiler->SynthCarry(SynthesisCarryInference::NoConst);
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
      compiler->ErrorMessage("Unknown option: " + option);
      return TCL_ERROR;
    }
    return TCL_OK;
  };
  interp->registerCmd("synth_options", synth_options, this, 0);

  return true;
}

std::string CompilerRS::BaseVprCommand() {
  std::string device_size = "";
  if (!m_deviceSize.empty()) {
    device_size = " --device " + m_deviceSize;
  }

  std::string netlistFile = m_projManager->projectName() + "_post_synth.blif";
  for (const auto& lang_file : m_projManager->DesignFiles()) {
    switch (m_projManager->designFileData(lang_file)) {
      case Design::Language::VERILOG_NETLIST:
      case Design::Language::BLIF:
      case Design::Language::EBLIF: {
        netlistFile = lang_file;
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

  std::string command =
      m_vprExecutablePath.string() + std::string(" ") +
      m_architectureFile.string() + std::string(" ") +
      std::string(
          netlistFile + std::string(" --sdc_file ") +
          std::string(m_projManager->projectName() + "_openfpga.sdc") +
          std::string(" --route_chan_width ") +
          std::to_string(m_channel_width) +
          " --clock_modeling ideal --timing_report_npaths 100 "
          "--absorb_buffer_luts off --constant_net_method route "
          "--timing_report_detail detailed --post_place_timing_report " +
          m_projManager->projectName() + "_post_place_timing.rpt" +
          device_size + pnrOptions);

  return command;
}

extern const char* foedag_version_number;
extern const char* foedag_git_hash;
void CompilerRS::Version(std::ostream* out) {
  (*out) << "RapidSilicon Raptor Compiler"
         << "\n";
  if (std::string(foedag_version_number) != "${VERSION_NUMBER}")
    (*out) << "Version : " << foedag_version_number << "\n";
  if (std::string(foedag_git_hash) != "${GIT_HASH}")
    (*out) << "Git Hash: " << foedag_git_hash << "\n";
  (*out) << "Built   : " << std::string(__DATE__) << "\n";
}

void CompilerRS::Help(std::ostream* out) {
  (*out) << "----------------------------------" << std::endl;
  (*out) << "--- RapidSilicon RAPTOR HELP  ----" << std::endl;
  (*out) << "----------------------------------" << std::endl;
  (*out) << "Options:" << std::endl;
  (*out) << "   --help           : This help" << std::endl;
  (*out) << "   --version        : Version" << std::endl;
  (*out) << "   --batch          : Tcl only, no GUI" << std::endl;
  (*out) << "   --script <script>: Execute a Tcl script" << std::endl;
  (*out) << "Tcl commands:" << std::endl;
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
  (*out) << "   add_include_path <path1>...: As in +incdir+" << std::endl;
  (*out) << "   add_library_path <path1>...: As in +libdir+" << std::endl;
  (*out) << "   set_macro <name>=<value>...: As in -D<macro>=<value>"
         << std::endl;
  (*out) << "   set_top_module <top>       : Sets the top module" << std::endl;
  (*out) << "   add_constraint_file <file> : Sets SDC + location constraints"
         << std::endl;
  (*out) << "                                Constraints: set_pin_loc, "
            "set_region_loc, all SDC commands"
         << std::endl;
  (*out) << "   ipgenerate                 : IP generation" << std::endl;
  (*out) << "   verific_parser <on/off>    : Turns on/off Verific Parser"
         << std::endl;
  (*out) << "   synthesis_type Yosys/QL/RS : Selects Synthesis type"
         << std::endl;
  (*out) << "   custom_synth_script <file> : Uses a custom Yosys templatized "
            "script"
         << std::endl;
  (*out)
      << "   synthesize <optimization>  : RTL Synthesis, optional opt. (area, "
         "delay, mixed, none)"
      << std::endl;
  (*out) << "   synth_options <option list>: RS-Yosys Plugin Options. "
            "The following defaults exist:"
         << std::endl;
  (*out) << "                              :   -effort high" << std::endl;
  (*out) << "                              :   -fsm_encoding binary if "
            "optimization == area else onehot"
         << std::endl;
  (*out) << "                              :   -carry no_const" << std::endl;
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
  (*out) << "       no_const               : Infer carries only with non "
            "constant inputs"
         << std::endl;
  (*out) << "       none                   : Do not infer carries" << std::endl;
  (*out) << "     -no_dsp                  : Do not use DSP blocks to "
            "implement multipliers and associated logic"
         << std::endl;
  (*out) << "     -no_bram                 : Do not use Block RAM to "
            "implement memory components"
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
  (*out) << "   packing                    : Packing" << std::endl;
  (*out) << "   global_placement           : Analytical placer" << std::endl;
  (*out) << "   place                      : Detailed placer" << std::endl;
  (*out) << "   route                      : Router" << std::endl;
  (*out) << "   sta                        : Statistical Timing Analysis"
         << std::endl;
  (*out) << "   power                      : Power estimator" << std::endl;
  (*out) << "   bitstream ?force?          : Bitstream generation" << std::endl;
  (*out) << "----------------------------------" << std::endl;
}
