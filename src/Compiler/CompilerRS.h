/*
Copyright 2022 RapidSilicon
All rights reserved
 */
#ifndef COMPILER_RS_H
#define COMPILER_RS_H

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Compiler/CompilerOpenFPGA.h"
#include "Utils/ArgumentsMap.h"

#ifdef PRODUCTION_BUILD
class License_Manager;
#endif

namespace FOEDAG {

class CompilerRS : public CompilerOpenFPGA {
 public:
  enum class SynthesisEffort { None, High, Low, Medium };
  enum class SynthesisCarryInference { None, NoCarry, All, Auto };
  enum class SynthesisIOInference { None, Auto };
  enum class SynthesisFsmEncoding { None, Binary, Onehot };
  enum class SynthesisClkeStrategy { None, Early, Late };

 public:
  CompilerRS();
  ~CompilerRS();

  std::vector<std::string> helpTags() const;
  void Version(std::ostream* out);
  std::string BaseVprCommand(BaseVprDefaults defaults);
  virtual std::string InitSynthesisScript();
  virtual std::string FinishSynthesisScript(const std::string& script);
  virtual std::string FinishAnalyzeScript(const std::string& script);
  virtual bool RegisterCommands(TclInterpreter* interp, bool batchMode);

  SynthesisEffort SynthEffort() { return m_synthEffort; }
  void SynthEffort(SynthesisEffort effort) { m_synthEffort = effort; }
  SynthesisCarryInference SynthCarry() { return m_synthCarry; }
  void SynthCarry(SynthesisCarryInference carry) { m_synthCarry = carry; }
  SynthesisIOInference SynthIO() { return m_synthIO; }
  SynthesisIOInference SynthIOUser() const { return m_userSynthIOSetting; }
  void SynthIO(SynthesisIOInference io) {
    m_synthIO = io;
    m_userSynthIOSetting = io;
  }
  SynthesisFsmEncoding SynthFsm() { return m_synthFsm; }
  void SynthFsm(SynthesisFsmEncoding fsmEnc) { m_synthFsm = fsmEnc; }
  SynthesisClkeStrategy SynthClke() { return m_synthClke; }
  void SynthClke(SynthesisClkeStrategy clkeStrategy) {
    m_synthClke = clkeStrategy;
  }
  bool SynthNoAdder() { return m_synthNoAdder; }
  void SynthNoAdder(bool noAdder) { m_synthNoAdder = noAdder; }
  bool SynthFast() { return m_synthFast; }
  void SynthFast(bool fast) { m_synthFast = fast; }
  bool SynthNoFlatten() { return m_synthNoFlatten; }
  void SynthNoFlatten(bool no_flatten) { m_synthNoFlatten = no_flatten; }
  bool SynthCec() { return m_synthCec; }
  void SynthCec(bool cec) { m_synthCec = cec; }
  bool SynthNoSimplify() { return m_synthNoSimplify; }
  void SynthNoSimplify(bool noSimplify) { m_synthNoSimplify = noSimplify; }
  int MaxThreads() { return m_maxThreads; }
  void MaxThreads(int maxThreads) { m_maxThreads = maxThreads; }
  bool SynthNoSat() { return m_synthNoSat; }
  void SynthNoSat(bool no_sat) { m_synthNoSat = no_sat; }
  int SynthInitRegisters() { return m_synthInitRegisters; }
  void SynthInitRegisters(int initRegisters) {
    m_synthInitRegisters = initRegisters;
  }

  void StarsExecPath(const std::filesystem::path& path) {
    m_starsExecutablePath = path;
  }
  void TclExecPath(const std::filesystem::path& path) {
    m_tclExecutablePath = path;
  }
  void RaptorExecPath(const std::filesystem::path& path) {
    m_raptorExecutablePath = path;
  }
  bool TimingAnalysis();
  bool PowerAnalysis();

  bool KeepTribuf() const { return m_keepTribuf; }
  void KeepTribuf(bool newKeepTribuf) { m_keepTribuf = newKeepTribuf; }

  bool NewDsp19x2() const { return m_new_dsp19x2; }
  void NewDsp19x2(bool new_dsp19x2) { m_new_dsp19x2 = new_dsp19x2; }

  bool NewTdp36k() const { return m_new_tdp36k; }
  void NewTdp36k(bool new_tdp36k) { m_new_tdp36k = new_tdp36k; }

  bool NewIOBufMap() const { return m_new_ioBufMap; }
  void NewIOBufMap(bool new_ioBufMap) { m_new_ioBufMap = new_ioBufMap; }

  virtual void adjustTargetDeviceDefaults();

 protected:
  void CustomSimulatorSetup(Simulator::SimulationType action);
  bool LicenseDevice(const std::string& deviceName);
  virtual std::string BaseStaCommand();
  virtual std::string BaseStaScript(std::string libFileName,
                                    std::string netlistFileName,
                                    std::string sdfFileName,
                                    std::string sdcFileName);
  SynthesisEffort m_synthEffort = SynthesisEffort::High;
  SynthesisCarryInference m_synthCarry = SynthesisCarryInference::Auto;
  SynthesisFsmEncoding m_synthFsm = SynthesisFsmEncoding::Onehot;
  SynthesisClkeStrategy m_synthClke = SynthesisClkeStrategy::None;
  SynthesisIOInference m_synthIO = SynthesisIOInference::Auto;
  SynthesisIOInference m_userSynthIOSetting = SynthesisIOInference::None;
  bool m_synthNoAdder = false;
  bool m_synthFast = false;
  bool m_synthNoFlatten = false;
  bool m_synthCec = false;
  bool m_synthNoSimplify = false;
  int m_maxThreads = -1;
  int m_synthInitRegisters = 0;
  bool m_keepTribuf = true;
  bool m_new_dsp19x2 = true;
  bool m_new_tdp36k = true;
  bool m_new_ioBufMap = true;
  bool m_synthNoSat = false;
  std::filesystem::path m_starsExecutablePath = "planning";
  std::filesystem::path m_tclExecutablePath = "tclsh";
  std::filesystem::path m_raptorExecutablePath = "raptor";
#ifdef PRODUCTION_BUILD
  License_Manager* licensePtr = nullptr;
#endif
};

ArgumentsMap TclArgs_getRsSynthesisOptions();
void TclArgs_setRsSynthesisOptions(const ArgumentsMap& argsStr);

}  // namespace FOEDAG

#endif
