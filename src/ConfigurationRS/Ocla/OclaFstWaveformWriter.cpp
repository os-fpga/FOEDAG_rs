#include "OclaFstWaveformWriter.h"

#include <time.h>

#include <iostream>
#include <map>
#include <string>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "fstapi.h"

#define FST_TS_S 0
#define FST_TS_MS -3
#define FST_TS_US -6
#define FST_TS_NS -9
#define FST_TS_PS -12

struct fst_signal_var_t {
  fstHandle handle;
  oc_signal_t& signal;
};

bool OclaFstWaveformWriter::write(oc_waveform_t& waveform,
                                  std::string filepath) {
  std::vector<fst_signal_var_t> fst_signals{};
  uint32_t max_depth = 0;

  void* fst = fstWriterCreate(filepath.c_str(), /* use_compressed_hier */ 1);
  if (!fst) {
    CFG_POST_ERR("Fail to create output file '%s'", filepath.c_str());
    return false;
  }

  // create header info
  fstWriterSetPackType(fst, FST_WR_PT_LZ4);
  fstWriterSetTimescale(fst, FST_TS_NS);
  fstWriterSetComment(fst, "Created by RapidSilicon OCLA Debugger Tool");
  fstWriterSetDate(fst, CFG_get_time().c_str());

  // create signal groups
  fstWriterSetScope(fst, FST_ST_VCD_PROGRAM, "OCLA Debugger", NULL);
  std::string name =
      std::string("Clock Domain ") + std::to_string(waveform.domain_id);
  fstWriterSetScope(fst, FST_ST_VCD_MODULE, name.c_str(), NULL);

  // create signals variables for each probe
  for (auto& probe : waveform.probes) {
    std::string probe_name =
        std::string("Probe ") + std::to_string(probe.probe_id);
    fstWriterSetScope(fst, FST_ST_VCD_FUNCTION, probe_name.c_str(), NULL);

    for (auto& signal : probe.signal_list) {
      max_depth = std::max(max_depth, signal.depth);
      fstHandle var = fstWriterCreateVar(fst, FST_VT_VCD_WIRE, FST_VD_INPUT,
                                         signal.bitwidth, signal.name.c_str(),
                                         /* alias */ 0);
      fst_signals.push_back({var, signal});
    }
    fstWriterSetUpscope(fst);
  }

  // write waveform
  for (uint32_t i = 0; i < max_depth; i++) {
    fstWriterEmitTimeChange(fst, i);
    for (auto& var : fst_signals) {
      if (i < var.signal.depth) {
        fstWriterEmitValueChangeVec32(
            fst, var.handle, var.signal.bitwidth,
            &var.signal.values.at(i * var.signal.words_per_line));
      }
    }
  }

  fstWriterClose(fst);

  return true;
}
