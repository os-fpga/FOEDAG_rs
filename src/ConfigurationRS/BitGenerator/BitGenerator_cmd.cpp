/*
Copyright 2021 The Foedag team

GPL License

Copyright (c) 2021 The Open-Source FPGA Foundation

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BitGenerator.h"
#include "CFGCommon/CFGArg_auto.h"
#include "CFGCommonRS/CFGCommonRS.h"

int main(int argc, const char** argv) {
  CFGArg_BITGEN bitgen_arg;
  if (bitgen_arg.parse(argc - 1, &argv[1]) && !bitgen_arg.Help) {
    CFGCommon_ARG cmdarg;
    cmdarg.arg = &bitgen_arg;
    BitGenerator_entry(&cmdarg);
  }
  return 0;
}
