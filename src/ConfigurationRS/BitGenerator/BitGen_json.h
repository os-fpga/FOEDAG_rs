#ifndef BITGEN_JSON_H
#define BITGEN_JSON_H

#include "BitGen_packer.h"
#include "CFGCommonRS/CFGCommonRS.h"

class BitGen_JSON {
 public:
  static void parse_bitstream(const std::string& filepath,
                              std::vector<BitGen_BITSTREAM_BOP*>& bops);
};

#endif
