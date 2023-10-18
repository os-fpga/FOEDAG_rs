#include "FstWaveformWriter.h"

#include <time.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "fstapi.h"

#define FST_TS_S 0
#define FST_TS_MS -3
#define FST_TS_US -6
#define FST_TS_NS -9
#define FST_TS_PS -12

#define OCLA_MAX_WIDTH (1024U)  // OCLA supports max of 1024 probes

uint32_t CFG_read_bit_vec32(uint32_t* data, uint32_t pos) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  return data[pos / 32] >> (pos % 32) & 1;
}

void CFG_write_bit_vec32(uint32_t* data, uint32_t pos, uint32_t value) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  data[pos / 32] |= value << (pos % 32);
}

void CFG_read_bitfield_vec32(uint32_t* data, uint32_t pos, uint32_t bitwidth,
                             uint32_t* output) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(output != nullptr);
  for (uint32_t i = 0; i < bitwidth; i++) {
    CFG_write_bit_vec32(output, i, CFG_read_bit_vec32(data, pos + i));
  }
}

void FstWaveformWriter::write(std::vector<uint32_t> values,
                              std::string filepath) {
  CFG_ASSERT_MSG(m_signals.empty() == false, "No signal info defined");
  CFG_ASSERT_MSG(m_width <= OCLA_MAX_WIDTH,
                 "Width is larger than max size of %d", OCLA_MAX_WIDTH);

  uint32_t words_per_line = ((m_width - 1) / 32) + 1;
  uint32_t expected_size = words_per_line * m_depth;
  uint32_t total_bit_width = count_total_bitwidth();

  CFG_ASSERT_MSG(expected_size == values.size(),
                 "Size of values vector is invalid. Expected size is %d",
                 expected_size);
  CFG_ASSERT_MSG(total_bit_width == m_width,
                 "Total bitwidth %d is not equal to line width %d",
                 total_bit_width, m_width);

  void* fst = fstWriterCreate(filepath.c_str(), /* use_compressed_hier */ 1);
  CFG_ASSERT_MSG(fst != nullptr, "Fail to create output file %s",
                 filepath.c_str());

  // header info
  fstWriterSetPackType(fst, FST_WR_PT_LZ4);
  fstWriterSetTimescale(fst, FST_TS_NS);
  fstWriterSetComment(fst, "Created by RapidSilicon OCLA Debugger Tool");
  fstWriterSetDate(fst, CFG_get_time().c_str());
  fstWriterSetScope(fst, FST_ST_VCD_MODULE, "ocla", NULL);

  // create fst signal vars
  std::vector<fstHandle> fst_signals;

  for (auto& sig : m_signals) {
    fstHandle var =
        fstWriterCreateVar(fst, FST_VT_VCD_WIRE, FST_VD_INPUT, sig.bitwidth,
                           sig.name.c_str(), /* alias */ 0);
    fst_signals.push_back(var);
  }

  // write fst waveform
  for (uint32_t i = 0, n = 0; i < m_depth; ++i, n += words_per_line) {
    fstWriterEmitTimeChange(fst, i);

    uint32_t offset = 0;
    uint32_t j = 0;

    for (auto& sig : m_signals) {
      uint32_t data[OCLA_MAX_WIDTH / 32] = {0};
      CFG_read_bitfield_vec32(&values.at(n), offset, sig.bitwidth, &data[0]);
      fstWriterEmitValueChangeVec32(fst, fst_signals[j++], sig.bitwidth,
                                    &data[0]);
      offset += sig.bitwidth;
    }
  }

  fstWriterClose(fst);
}

uint32_t FstWaveformWriter::count_total_bitwidth() {
  uint32_t bitwidth = 0;
  for (auto& sig : m_signals) {
    bitwidth += sig.bitwidth;
  }
  return bitwidth;
}
