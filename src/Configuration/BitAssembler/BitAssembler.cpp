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

#include "Utils/FileUtils.h"
#include "CFGCommon/CFGHelper.h"
#include "CFGObject/CFGObject_auto.h"
#include "BitAssembler.h"
#include "BitAssembler_mgr.h"

void BitAssembler_entry(const BitAssemblerArg& args, CFGMessager& msger)
{
    CFG_TIME time_begin = CFG_time_begin();
	msger.add_msg("This is BITASM entry function");
    msger.add_msg(CFG_print("   project: %s", args.project_name.c_str()));
    msger.add_msg(CFG_print("   device: %s", args.device_name.c_str()));
    msger.add_msg(CFG_print("   time: %s", CFG_get_time().c_str()));
    std::string bitasm_file = CFG_print("%s/%s.bitasm", args.project_path.c_str(), args.project_name.c_str());
    msger.add_msg(CFG_print("   Output: %s", bitasm_file.c_str()));
    msger.add_msg(CFG_print("   Clean: %d", args.clean));
    if (args.clean)
    {
        FOEDAG::FileUtils::removeFile(bitasm_file);
    }
    else
    {
        CFGObject_BITOBJ bitobj;
        bitobj.write_str("version", "Raptor 1.0");
        bitobj.write_str("project", args.project_name);
        bitobj.write_str("device", args.device_name);
        bitobj.write_str("time", CFG_get_time());
        
        // FCB
        BitAssembler_MGR mgr(args.project_path, args.device_name);
        mgr.get_fcb(&bitobj.fcb);

        // Writing out
        msger.add_msg(CFG_print("   BITOBJ write: %d", bitobj.write(bitasm_file)));
        for (auto msg : bitobj.error_msgs)
        {
            msger.add_msg(CFG_print("   Error: %s", msg.c_str()));
        }
    }
    msger.add_msg(CFG_print("Assembler elapsed time: %.3f seconds", CFG_time_elapse(time_begin)));
}