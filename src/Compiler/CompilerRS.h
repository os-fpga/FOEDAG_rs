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
  std::string BaseVprCommand();

 protected:
};

}  // namespace FOEDAG

#endif
