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
${PLUGIN_NAME} -family ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION}

# Clean and output blif
write_blif ${OUTPUT_BLIF}
write_verilog -noexpr -nodec -defparam -norename ${OUTPUT_VERILOG}
  )";

const std::string RapidSiliconYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}

# Technology mapping
hierarchy -top ${TOP_MODULE}

${KEEP_NAMES}

plugin -i ${PLUGIN_LIB}
${PLUGIN_NAME} -tech ${MAP_TO_TECHNOLOGY} -top ${TOP_MODULE} ${OPTIMIZATION}

# Clean and output blif
write_blif ${OUTPUT_BLIF}
write_verilog -noexpr -nodec -defparam -norename ${OUTPUT_VERILOG}
  )";

std::string CompilerRS::InitSynthesisScript() {
  switch (m_synthType) {
    case SynthesisType::Yosys:
      return CompilerOpenFPGA::InitSynthesisScript();
    case SynthesisType::QL: {
      m_mapToTechnology = "qlf_k6n10";
      m_yosysPluginLib = "ql-qlf";
      m_yosysPlugin = "synth_ql";
      YosysScript(QLYosysScript);
      break;
    }
    case SynthesisType::RS: {
      m_mapToTechnology = "genesis";
      m_yosysPluginLib = "synth-rs";
      m_yosysPlugin = "synth_rs";
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
    (*m_out) << "Keep name: " << keep << "\n";
    keeps += "setattr -set keep 1 " + keep + "\n";
  }
  result = ReplaceAll(result, "${KEEP_NAMES}", keeps);
  std::string optimization;
  switch (m_synthOpt) {
    case SynthesisOpt::None:
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
  if (m_synthType == SynthesisType::QL) {
    optimization = "";
  }
  result = ReplaceAll(result, "${OPTIMIZATION}", optimization);
  result = ReplaceAll(result, "${PLUGIN_LIB}", PluginLibName());
  result = ReplaceAll(result, "${PLUGIN_NAME}", PluginName());
  result = ReplaceAll(result, "${MAP_TO_TECHNOLOGY}", MapTechnology());
  result = ReplaceAll(result, "${LUT_SIZE}", std::to_string(m_lut_size));
  return result;
}

CompilerRS::CompilerRS() { m_channel_width = 180; }

bool CompilerRS::RegisterCommands(TclInterpreter* interp, bool batchMode) {
  CompilerOpenFPGA::RegisterCommands(interp, batchMode);
  auto synthesis_type = [](void* clientData, Tcl_Interp* interp, int argc,
                           const char* argv[]) -> int {
    CompilerRS* compiler = (CompilerRS*)clientData;
    std::string name;
    if (argc != 2) {
      compiler->ErrorMessage("Specify type: Yosys/RS/QL");
      return TCL_ERROR;
    }
    std::string arg = argv[1];
    if (arg == "Yosys") {
      compiler->SynthType(SynthesisType::Yosys);
    } else if (arg == "RS") {
      compiler->SynthType(SynthesisType::RS);
    } else if (arg == "QL") {
      compiler->SynthType(SynthesisType::QL);
    } else {
      compiler->ErrorMessage("Illegal synthesis type: " + arg);
      return TCL_ERROR;
    }
    return TCL_OK;
  };
  interp->registerCmd("synthesis_type", synthesis_type, this, 0);

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
  (*out) << "   target_device <name>       : Targets a device with <name> name"
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
  (*out) << "   synth_options <option list>: RS-Yosys Plugin Options"
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
