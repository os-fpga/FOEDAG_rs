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

#ifndef BITASSEMBLER_H
#define BITASSEMBLER_H

#include "Configuration/CFGCommon/CFGMessager.h"

struct BitAssemblerArg {
  std::string project_path = "";
  std::string project_name = "";
  std::string device_name = "";
  bool clean = false;
};

void BitAssembler_entry(const BitAssemblerArg& args, CFGMessager& msger);

#endif
