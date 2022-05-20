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
  enum class SynthesisEffort { None, High, Low, Medium };
  enum class SynthesisCarryInference { None, NoCarry, All, NoConst };
  enum class SynthesisFsmEncoding { None, Binary, Onehot };

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
  SynthesisEffort getSynthEffort() { return m_synthEffort; }
  void setSynthEffort(SynthesisEffort effort) { m_synthEffort = effort; }
  SynthesisCarryInference getSynthCarry() { return m_synthCarry; }
  void setSynthCarry(SynthesisCarryInference carry) { m_synthCarry = carry; }
  SynthesisFsmEncoding getSynthFsm() { return m_synthFsm; }
  void setSynthFsm(SynthesisFsmEncoding fsmEnc) { m_synthFsm = fsmEnc; }
  bool getSynthNoDsp() { return m_synthNoDsp; }
  void setSynthNoDsp(bool noDsp) { m_synthNoDsp = noDsp; }
  bool getSynthNoBram() { return m_synthNoBram; }
  void setSynthNoBram(bool noBram) { m_synthNoBram = noBram; }

 protected:
  bool m_use_rs_synthesis = true;
  SynthesisEffort m_synthEffort = SynthesisEffort::None;
  SynthesisCarryInference m_synthCarry = SynthesisCarryInference::None;
  SynthesisFsmEncoding m_synthFsm = SynthesisFsmEncoding::None;
  bool m_synthNoDsp = false;
  bool m_synthNoBram = false;
};

}  // namespace FOEDAG

#endif
