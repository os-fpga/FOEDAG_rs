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

#ifdef PRODUCTION_BUILD
class License_Manager;
#endif

namespace FOEDAG {

class CompilerRS : public CompilerOpenFPGA {
 public:
  enum class SynthesisEffort { None, High, Low, Medium };
  enum class SynthesisCarryInference { None, NoCarry, All, Auto };
  enum class SynthesisFsmEncoding { None, Binary, Onehot };
  enum class SynthesisClkeStrategy { None, Early, Late };

 public:
  CompilerRS();
  ~CompilerRS();

  void Help(std::ostream* out);
  void Version(std::ostream* out);
  std::string BaseVprCommand();
  virtual std::string InitSynthesisScript();
  virtual std::string FinishSynthesisScript(const std::string& script);
  virtual std::string FinishAnalyzeScript(const std::string& script);
  virtual bool RegisterCommands(TclInterpreter* interp, bool batchMode);

  SynthesisEffort SynthEffort() { return m_synthEffort; }
  void SynthEffort(SynthesisEffort effort) { m_synthEffort = effort; }
  SynthesisCarryInference SynthCarry() { return m_synthCarry; }
  void SynthCarry(SynthesisCarryInference carry) { m_synthCarry = carry; }
  SynthesisFsmEncoding SynthFsm() { return m_synthFsm; }
  void SynthFsm(SynthesisFsmEncoding fsmEnc) { m_synthFsm = fsmEnc; }
  SynthesisClkeStrategy SynthClke() { return m_synthClke; }
  void SynthClke(SynthesisClkeStrategy clkeStrategy) {
    m_synthClke = clkeStrategy;
  }
  bool SynthNoDsp() { return m_synthNoDsp; }
  void SynthNoDsp(bool noDsp) { m_synthNoDsp = noDsp; }
  bool SynthNoBram() { return m_synthNoBram; }
  void SynthNoBram(bool noBram) { m_synthNoBram = noBram; }
  bool SynthNoAdder() { return m_synthNoAdder; }
  void SynthNoAdder(bool noAdder) { m_synthNoAdder = noAdder; }
  bool SynthFast() { return m_synthFast; }
  void SynthFast(bool fast) { m_synthFast = fast; }
  bool SynthCec() { return m_synthCec; }
  void SynthCec(bool cec) { m_synthCec = cec; }
  bool SynthNoSimplify() { return m_synthNoSimplify; }
  void SynthNoSimplify(bool noSimplify) { m_synthNoSimplify = noSimplify; }
  int MaxThreads() { return m_maxThreads; }
  void MaxThreads(int maxThreads) { m_maxThreads = maxThreads; }

  void StarsExecPath(const std::filesystem::path& path) {
    m_starsExecutablePath = path;
  }
  bool TimingAnalysis();
  virtual std::vector<std::string> GetCleanFiles(
      Action action, const std::string& projectName,
      const std::string& topModule) const;

 protected:
  void CustomSimulatorSetup(Simulator::SimulationType action);
  bool LicenseDevice(const std::string& deviceName);
  virtual std::string BaseStaCommand();
  virtual std::string BaseStaScript(std::string libFileName,
                                    std::string netlistFileName,
                                    std::string sdfFileName,
                                    std::string sdcFileName);
  SynthesisEffort m_synthEffort = SynthesisEffort::None;
  SynthesisCarryInference m_synthCarry = SynthesisCarryInference::None;
  SynthesisFsmEncoding m_synthFsm = SynthesisFsmEncoding::None;
  SynthesisClkeStrategy m_synthClke = SynthesisClkeStrategy::None;
  bool m_synthNoDsp = false;
  bool m_synthNoBram = false;
  bool m_synthNoAdder = false;
  bool m_synthFast = false;
  bool m_synthCec = false;
  bool m_synthNoSimplify = false;
  int m_maxThreads = -1;
  std::filesystem::path m_starsExecutablePath = "stars";
#ifdef PRODUCTION_BUILD
  License_Manager* licensePtr = nullptr;
#endif
};

std::string TclArgs_getRsSynthesisOptions();
void TclArgs_setRsSynthesisOptions(const std::string& argsStr);

}  // namespace FOEDAG

#endif
