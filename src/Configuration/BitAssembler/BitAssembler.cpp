/*
Copyright 2022 The Foedag team

GPL License

Copyright (c) 2022 The Open-Source FPGA Foundation

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

#include "BitAssembler.h"

#include "BitAssembler_mgr.h"
#include "CFGCommon/CFGHelper.h"
#include "CFGObject/CFGObject_auto.h"
#include "Utils/FileUtils.h"

void BitAssembler_entry(const BitAssemblerArg& args) {
  CFG_TIME time_begin = CFG_time_begin();
  std::string bitasm_time = CFG_get_time();
  CFG_POST_MSG("This is BITASM entry function");
  CFG_POST_MSG("   Project: %s", args.project_name.c_str());
  CFG_POST_MSG("   Device: %s", args.device_name.c_str());
  CFG_POST_MSG("   Time: %s", bitasm_time.c_str());
  std::string bitasm_file = CFG_print("%s/%s.bitasm", args.project_path.c_str(),
                                      args.project_name.c_str());
  CFG_POST_MSG("   Output: %s", bitasm_file.c_str());
  CFG_POST_MSG("   Operation: %s", (args.clean ? "clean" : "generate"));
  if (args.clean) {
    FOEDAG::FileUtils::removeFile(bitasm_file);
  } else {
    CFGObject_BITOBJ bitobj;
    bitobj.write_str("version", "Raptor 1.0");
    bitobj.write_str("project", args.project_name);
    bitobj.write_str("device", args.device_name);
    bitobj.write_str("time", bitasm_time);

    // FCB
    BitAssembler_MGR mgr(args.project_path, args.device_name);
    mgr.get_fcb(&bitobj.fcb);

    // Writing out
    CFG_POST_MSG("   Status: %s", bitobj.write(bitasm_file) ? "success" : "fail");
  }
  CFG_POST_MSG("BITASM elapsed time: %.3f seconds", CFG_time_elapse(time_begin));
}
