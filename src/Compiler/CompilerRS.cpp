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

using namespace FOEDAG;

const std::string RapidSiliconYosysScript = R"( 
# Yosys synthesis script for ${TOP_MODULE}
# Read source files
${READ_DESIGN_FILES}

# Technology mapping
hierarchy -top ${TOP_MODULE}

${KEEP_NAMES}

plugin -i synth-rs
synth_rs -tech genesis -top ${TOP_MODULE} ${OPTIMIZATION}

# Clean and output blif
write_blif ${OUTPUT_BLIF}
write_verilog ${OUTPUT_VERILOG}
  )";

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
    case NoOpt:
      break;
    case Area:
      optimization = "-de -goal area";
      break;
    case Delay:
      optimization = "-de -goal delay";
      break;
    case Mixed:
      optimization = "-de -goal mixed";
      break;
  }
  result = ReplaceAll(result, "${OPTIMIZATION}", optimization);
  result = ReplaceAll(result, "${LUT_SIZE}", std::to_string(m_lut_size));
  return result;
}

CompilerRS::CompilerRS() {
  YosysScript(RapidSiliconYosysScript);
  m_channel_width = 300;
}

std::string CompilerRS::BaseVprCommand() {
  std::string command =
      m_vprExecutablePath.string() + std::string(" ") +
      m_architectureFile.string() + std::string(" ") +
      std::string(m_design->Name() + "_post_synth.blif" +
                  std::string(" --sdc_file ") +
                  std::string(m_design->Name() + "_openfpga.sdc") +
                  std::string(" --route_chan_width ") +
                  std::to_string(m_channel_width) +
                  " --clock_modeling ideal --timing_report_npaths 100 "
                  "--absorb_buffer_luts off --constant_net_method route "
                  "--timing_report_detail detailed --post_place_timing_report "
                  "POST_PLACE_TIMING_REPORT");

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
  (*out) << "   --replay <script>: Replay GUI test" << std::endl;
  (*out) << "   --script <script>: Execute a Tcl script" << std::endl;
  (*out) << "Tcl commands:" << std::endl;
  (*out) << "   help                       : This help" << std::endl;
  (*out) << "   create_design <name>       : Creates a design with <name> name"
         << std::endl;
  (*out) << "   architecture <file>        : Uses the architecture file"
         << std::endl;
  (*out) << "   custom_synth_script <file> : Uses a custom Yosys templatized "
            "script"
         << std::endl;
  (*out) << "   set_channel_width <int>    : VPR Routing channel setting"
         << std::endl;
  (*out) << "   add_design_file <file>... <type> (-VHDL_1987, -VHDL_1993, "
            "-VHDL_2000, "
            "-VHDL_2008, -V_1995, "
            "-V_2001, -SV_2005, -SV_2009, -SV_2012, -SV_2017) "
         << std::endl;
  (*out) << "   add_include_path <path1>...: As in +incdir+" << std::endl;
  (*out) << "   add_library_path <path1>...: As in +libdir+" << std::endl;
  (*out) << "   set_macro <name>=<value>...: As in -D<macro>=<value>"
         << std::endl;
  (*out) << "   set_top_module <top>       : Sets the top module" << std::endl;
  (*out) << "   add_constraint_file <file> : Sets SDC + location constraints"
         << std::endl;
  (*out) << "     Constraints: set_pin_loc, set_region_loc, all SDC commands"
         << std::endl;
  (*out) << "   ipgenerate" << std::endl;
  (*out) << "   synthesize <optimization>  : Optional optimization (area, "
            "delay, mixed, none)"
         << std::endl;
  (*out) << "   packing" << std::endl;
  (*out) << "   global_placement" << std::endl;
  (*out) << "   place" << std::endl;
  (*out) << "   route" << std::endl;
  (*out) << "   sta" << std::endl;
  (*out) << "   power" << std::endl;
  (*out) << "   bitstream" << std::endl;
  (*out) << "----------------------------------" << std::endl;
}
