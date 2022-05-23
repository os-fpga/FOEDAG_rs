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
enum class SynthesisType { Yosys, QL, RS };

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
  void SynthType(SynthesisType type) { m_synthType = type; }
  SynthesisEffort SynthEffort() { return m_synthEffort; }
  void SynthEffort(SynthesisEffort effort) { m_synthEffort = effort; }
  SynthesisCarryInference SynthCarry() { return m_synthCarry; }
  void SynthCarry(SynthesisCarryInference carry) { m_synthCarry = carry; }
  SynthesisFsmEncoding SynthFsm() { return m_synthFsm; }
  void SynthFsm(SynthesisFsmEncoding fsmEnc) { m_synthFsm = fsmEnc; }
  bool SynthNoDsp() { return m_synthNoDsp; }
  void SynthNoDsp(bool noDsp) { m_synthNoDsp = noDsp; }
  bool SynthNoBram() { return m_synthNoBram; }
  void SynthNoBram(bool noBram) { m_synthNoBram = noBram; }
  bool SynthNoAdder() { return m_synthNoAdder; }
  void SynthNoAdder(bool noAdder) { m_synthNoAdder = noAdder; }

 protected:
  SynthesisType m_synthType = SynthesisType::RS;
  SynthesisEffort m_synthEffort = SynthesisEffort::None;
  SynthesisCarryInference m_synthCarry = SynthesisCarryInference::None;
  SynthesisFsmEncoding m_synthFsm = SynthesisFsmEncoding::None;
  bool m_synthNoDsp = false;
  bool m_synthNoBram = false;
  bool m_synthNoAdder = false;
};

}  // namespace FOEDAG

#endif
