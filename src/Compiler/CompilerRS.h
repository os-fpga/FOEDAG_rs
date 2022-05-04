/*
Copyright 2022 RapidSilicon
All rights reserved
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Compiler/CompilerOpenFPGA.h"

#ifndef COMPILER_RS_H
#define COMPILER_RS_H

namespace FOEDAG {
class CompilerRS : public CompilerOpenFPGA {
 public:
  CompilerRS();
  ~CompilerRS() = default;

  void Help(std::ostream* out);
  void Version(std::ostream* out);
  std::string BaseVprCommand();
  virtual std::string InitSynthesisScript();
  virtual std::string FinishSynthesisScript(const std::string& script);
  virtual bool RegisterCommands(TclInterpreter* interp, bool batchMode);
  void UseRsSynthesis(bool on) { m_use_rs_synthesis = on; }

 protected:
  bool m_use_rs_synthesis = true;
};

}  // namespace FOEDAG

#endif
