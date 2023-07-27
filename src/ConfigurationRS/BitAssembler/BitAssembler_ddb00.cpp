#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

#include "BitAssembler_ddb.h"
#include "BitAssembler_mgr.h"
#include "CFGCommonRS/CFGCommonRS.h"

const std::vector<std::string> BitAssembler_DDB_IP_00_GROUP1 = {
    "grid_clb",      "grid_io_bottom", "grid_io_top", "grid_io_left",
    "grid_io_right", "grid_dsp",       "grid_bram"};

const std::map<std::string, std::vector<std::string>>
    BitAssembler_DDB_IP_00_GROUP2 = {
        {"cbx", {"cbx_1__1_", "cbx_2__2_"}},
        {"cby", {"cby_1__1_", "cby_2__2_"}},
};

struct BitAssembler_DDB_IP_INFO_00 {
  BitAssembler_DDB_IP_INFO_00(uint32_t b) : bits(b) {
    bl_size = 1;
    wl_size = 1;
    while ((bl_size * wl_size) < bits) {
      bl_size++;
      if ((bl_size * wl_size) < bits) {
        wl_size++;
      }
    }
  }
  const uint32_t bits;
  uint32_t bl_size;
  uint32_t wl_size;
  uint32_t count = 0;
  std::vector<std::string> paths;
};

struct BitAssembler_DDB_IP_00 {
  BitAssembler_DDB_IP_00(std::string a) : alias(a) {
    CFG_ASSERT(alias.size());
    std::vector<std::string> words = CFG_split_string(alias, "__");
    CFG_ASSERT(words.size() == 2);
    size_t index = words[0].rfind("_");
    CFG_ASSERT(index != std::string::npos);
    type = words[0].substr(0, index);
    logical_col =
        (uint32_t)(CFG_convert_string_to_u64(words[0].substr(index + 1)));
    CFG_ASSERT(words[1][words[1].size() - 1] == '_');
    words[1].pop_back();
    logical_row = (uint32_t)(CFG_convert_string_to_u64(words[1]));
    if (type == "sb") {
      col = logical_col * 2 + 1;
      row = logical_row * 2 + 1;
    } else if (type == "cbx") {
      col = logical_col * 2;
      row = logical_row * 2 + 1;
    } else if (type == "cby") {
      col = logical_col * 2 + 1;
      row = logical_row * 2;
    } else {
      CFG_ASSERT_MSG(
          CFG_find_string_in_vector(BitAssembler_DDB_IP_00_GROUP1, type) >= 0,
          "Unsupported IP type %s", type.c_str());
      col = logical_col * 2;
      row = logical_row * 2;
    }
  }
  BitAssembler_DDB_IP_00(std::string n, uint32_t i, uint32_t v, uint32_t c,
                         uint32_t r)
      : name(n), id(i), value(v), col(c), row(r) {
    if (CFG_find_string_in_vector(BitAssembler_DDB_IP_00_GROUP1, name) >= 0) {
      type = name;
      CFG_ASSERT((col & 1) == 0);
      CFG_ASSERT((row & 1) == 0);
    } else {
      std::vector<std::string> words = CFG_split_string(name, "__");
      CFG_ASSERT(words.size() == 2);
      size_t index = words[0].rfind("_");
      CFG_ASSERT(index != std::string::npos);
      type = words[0].substr(0, index);
      if (type == "sb") {
        CFG_ASSERT((col & 1) != 0);
        CFG_ASSERT((row & 1) != 0);
      } else if (type == "cbx") {
        CFG_ASSERT((col & 1) == 0);
        CFG_ASSERT((row & 1) != 0);
      } else {
        CFG_ASSERT(type == "cby");
        CFG_ASSERT((col & 1) != 0);
        CFG_ASSERT((row & 1) == 0);
      }
    }
    logical_col = col / 2;
    logical_row = row / 2;
    alias = CFG_print("%s_%d__%d_", type.c_str(), logical_col, logical_row);
  }
  std::string alias;
  std::string type = "";
  std::string name = "";
  uint32_t id = uint32_t(-1);
  uint32_t value = uint32_t(-1);
  uint32_t col = 0;
  uint32_t row = 0;
  uint32_t logical_col = 0;
  uint32_t logical_row = 0;
};

struct BitAssembler_DDB_00 {
  ~BitAssembler_DDB_00() {
    // Memory Leak
    for (uint32_t c = 0; c < (uint32_t)(layout_ips.size()); c++) {
      for (uint32_t r = 0; r < (uint32_t)(layout_ips[c].size()); r++) {
        CFG_MEM_DELETE(layout_ips[c][r]);
      }
    }
    for (auto& iter : ip_infos) {
      CFG_MEM_DELETE(iter.second);
    }
  }
  void finalize_layout_ips() {
    CFG_ASSERT(layout_ips.size() == (size_t)(col_size));
    for (uint32_t i = 0; i < col_size; i++) {
      CFG_ASSERT(layout_ips[i].size() <= (size_t)(row_size));
      while (layout_ips[i].size() < (size_t)(row_size)) {
        layout_ips[i].push_back(nullptr);
      }
    }
    for (uint32_t i = 0; i < col_size; i++) {
      CFG_ASSERT(layout_ips[i][0] == nullptr);
    }
    for (uint32_t i = 0; i < row_size; i++) {
      CFG_ASSERT(layout_ips[0][i] == nullptr);
    }
  }
  void finalize_bl_wl() {
    CFG_ASSERT(bls.size());
    CFG_ASSERT(wls.size());
    CFG_ASSERT(bl == 0);
    CFG_ASSERT(wl == 0);
    CFG_ASSERT(acc_bls.size() == 0);
    CFG_ASSERT(acc_wls.size() == 0);
    for (auto& t : bls) {
      acc_bls.push_back(bl);
      bl += t;
    }
    for (auto& t : wls) {
      acc_wls.push_back(wl);
      wl += t;
    }
  }
  void update_bl_wl() {
    CFG_ASSERT(bls.size() == 0);
    CFG_ASSERT(wls.size() == 0);
    for (uint32_t c = 0; c < col_size; c++) {
      bls.push_back(0);
      for (uint32_t r = 0; r < row_size; r++) {
        if (layout_ips[c][r] != nullptr &&
            ip_infos[layout_ips[c][r]->name]->bl_size > bls[c]) {
          bls[c] = ip_infos[layout_ips[c][r]->name]->bl_size;
        }
      }
    }
    for (uint32_t r = 0; r < row_size; r++) {
      wls.push_back(0);
      for (uint32_t c = 0; c < col_size; c++) {
        if (layout_ips[c][r] != nullptr &&
            ip_infos[layout_ips[c][r]->name]->wl_size > wls[r]) {
          wls[r] = ip_infos[layout_ips[c][r]->name]->wl_size;
        }
      }
    }
    finalize_bl_wl();
  }
  void finalize_configuration_bits() {
    CFG_ASSERT(configuration_bits == 0);
    for (auto& iter : region_ips) {
      configuration_bits += ip_infos[iter->name]->bits;
    }
  }
  void create_blwls() {
    CFG_ASSERT(blwls.size() == 0);
    uint32_t bl_index = 0;
    uint32_t wl_index = 0;
    for (auto& iter : region_ips) {
      bl_index = acc_bls[iter->col];
      wl_index = acc_wls[iter->row];
      BitAssembler_DDB_IP_INFO_00* ip_info = ip_infos[iter->name];
      for (uint32_t i = 0, j = 0; i < ip_info->bits; i++) {
        blwls.push_back(std::make_pair(bl_index, wl_index));
        j++;
        bl_index++;
        if (j == ip_info->bl_size) {
          j = 0;
          bl_index -= ip_info->bl_size;
          wl_index++;
        }
      }
    }
    CFG_ASSERT(blwls.size());
  }
  uint32_t col_size = 0;
  uint32_t row_size = 0;
  uint32_t max_logical_col = 0;
  uint32_t max_logical_row = 0;
  uint32_t bl = 0;
  uint32_t wl = 0;
  uint32_t configuration_bits = 0;
  std::map<std::string, BitAssembler_DDB_IP_INFO_00*> ip_infos;
  std::vector<std::vector<BitAssembler_DDB_IP_00*>> layout_ips;
  std::vector<BitAssembler_DDB_IP_00*> region_ips;
  std::vector<uint32_t> bls;
  std::vector<uint32_t> wls;
  std::vector<uint32_t> acc_bls;
  std::vector<uint32_t> acc_wls;
  std::vector<std::pair<uint32_t, uint32_t>> blwls;
};

void CFG_ddb_gen_database_00(const std::string& device,
                             const std::string& input_xml,
                             const std::string& output_ddb) {
  CFG_POST_MSG("Generate Distribution IPs");
  std::fstream file;
  file.open(input_xml.c_str(), std::ios::in);
  CFG_ASSERT_MSG(file.is_open(), "Fail to open %s", input_xml.c_str());
  std::string line = "";
  size_t index = 0;
  size_t end_index = 0;
  uint32_t id = 0;
  std::vector<std::string> paths;
  std::map<std::string, uint32_t> distribution_ips;
  std::vector<std::string> words;
  while (getline(file, line)) {
    index = line.find("bit id=\"");
    if (index != std::string::npos) {
      index += 8;
      end_index = line.find("\"", index);
      CFG_ASSERT(end_index != std::string::npos);
      id = (uint32_t)(CFG_convert_string_to_u64(
          line.substr(index, end_index - index)));
      index = line.find("path=\"", end_index);
      CFG_ASSERT(index != std::string::npos);
      index += 6;
      end_index = line.find("\"", index);
      CFG_ASSERT(end_index != std::string::npos);
      words = CFG_split_string(line.substr(index, end_index - index), ".", 2);
      CFG_ASSERT(words.size() == 3);
      CFG_ASSERT(words[0] == "fpga_top");
      // ip bits
      if (distribution_ips.find(words[1]) == distribution_ips.end()) {
        distribution_ips[words[1]] = 0;
      }
      distribution_ips[words[1]]++;
      // paths
      while (paths.size() <= (size_t)(id)) {
        paths.push_back("");
      }
      CFG_ASSERT(paths[id] == "");
      paths[id] = words[2];
    }
  }
  file.close();
  CFG_POST_MSG("Generate Layout IPs");
  BitAssembler_DDB_00 ddb;
  BitAssembler_DDB_IP_00* ip = nullptr;
  for (auto& iter : distribution_ips) {
    ip = CFG_MEM_NEW(BitAssembler_DDB_IP_00, iter.first);
    while (ddb.layout_ips.size() <= ip->col) {
      ddb.layout_ips.push_back({});
    }
    while (ddb.layout_ips[ip->col].size() <= ip->row) {
      ddb.layout_ips[ip->col].push_back(nullptr);
    }
    CFG_ASSERT(ddb.layout_ips[ip->col][ip->row] == nullptr);
    ddb.layout_ips[ip->col][ip->row] = ip;
    if (ip->col > ddb.col_size) {
      ddb.col_size = ip->col;
    }
    if (ip->row > ddb.row_size) {
      ddb.row_size = ip->row;
    }
    if (ip->logical_col > ddb.max_logical_col) {
      ddb.max_logical_col = ip->logical_col;
    }
    if (ip->logical_row > ddb.max_logical_row) {
      ddb.max_logical_row = ip->logical_row;
    }
  }
  ddb.col_size++;
  ddb.row_size++;
  ddb.finalize_layout_ips();
  // Assign IP name
  CFG_POST_MSG("Assign IP name");
  auto assign_ip_info =
      [](std::map<std::string, BitAssembler_DDB_IP_INFO_00*>& ip_infos,
         BitAssembler_DDB_IP_00* const ip, std::string name,
         std::map<std::string, uint32_t>& distribution_ips) {
        CFG_ASSERT(ip != nullptr);
        CFG_ASSERT(ip->name.size() == 0);
        CFG_ASSERT(name.size());
        ip->name = name;
        if (ip_infos.find(ip->name) == ip_infos.end()) {
          ip_infos[ip->name] = CFG_MEM_NEW(BitAssembler_DDB_IP_INFO_00,
                                           distribution_ips[ip->alias]);
        }
        CFG_ASSERT_MSG(ip_infos[ip->name]->bits == distribution_ips[ip->alias],
                       "Conflict info: name %s (%d) vs alias %s (%d)",
                       ip->name.c_str(), ip_infos[ip->name]->bits,
                       ip->alias.c_str(), distribution_ips[ip->alias]);
      };
  auto find_ip =
      [](std::vector<std::vector<BitAssembler_DDB_IP_00*>>& layout_ips,
         const std::string& alias) {
        BitAssembler_DDB_IP_00* ip = nullptr;
        for (uint32_t c = 0; c < (uint32_t)(layout_ips.size()); c++) {
          for (uint32_t r = 0; r < (uint32_t)(layout_ips[c].size()); r++) {
            if (layout_ips[c][r] != nullptr &&
                layout_ips[c][r]->alias == alias) {
              ip = layout_ips[c][r];
              break;
            }
          }
          if (ip != nullptr) {
            break;
          }
        }
        CFG_ASSERT(ip != nullptr);
        return ip;
      };
  CFG_POST_MSG("  Group 1 (No column and no row)");
  for (uint32_t c = 0; c < ddb.col_size; c++) {
    for (uint32_t r = 0; r < ddb.row_size; r++) {
      if (ddb.layout_ips[c][r] != nullptr) {
        if (CFG_find_string_in_vector(BitAssembler_DDB_IP_00_GROUP1,
                                      ddb.layout_ips[c][r]->type) >= 0) {
          CFG_ASSERT(ddb.layout_ips[c][r]->name.size() == 0);
          assign_ip_info(ddb.ip_infos, ddb.layout_ips[c][r],
                         ddb.layout_ips[c][r]->type, distribution_ips);
        }
      }
    }
  }
  CFG_POST_MSG("  Group 2 (Outer and Inner)");
  for (auto& iter : BitAssembler_DDB_IP_00_GROUP2) {
    CFG_POST_MSG("    Type: %s", iter.first.c_str());
    CFG_ASSERT(iter.second.size() == 2);
    for (uint32_t c = 0; c < ddb.col_size; c++) {
      for (uint32_t r = 0; r < ddb.row_size; r++) {
        if (ddb.layout_ips[c][r] != nullptr &&
            ddb.layout_ips[c][r]->type == iter.first) {
          CFG_ASSERT(ddb.layout_ips[c][r]->name.size() == 0);
          if (ddb.layout_ips[c][r]->logical_col == 1 ||
              ddb.layout_ips[c][r]->logical_col == ddb.max_logical_col ||
              ddb.layout_ips[c][r]->logical_row == 1 ||
              ddb.layout_ips[c][r]->logical_row == ddb.max_logical_row) {
            assign_ip_info(ddb.ip_infos, ddb.layout_ips[c][r], iter.second[0],
                           distribution_ips);
          } else {
            assign_ip_info(ddb.ip_infos, ddb.layout_ips[c][r], iter.second[1],
                           distribution_ips);
          }
        }
      }
    }
  }
  CFG_POST_MSG("  Group 3 (Switch Box)");
  std::vector<std::string> IP_NAMES = {
      CFG_print("sb_0__%d_", ddb.max_logical_row),
      CFG_print("sb_%d__%d_", ddb.max_logical_col - 1, ddb.max_logical_row),
      CFG_print("sb_%d__0_", ddb.max_logical_col),
      CFG_print("sb_%d__%d_", ddb.max_logical_col, ddb.max_logical_row),
  };
  CFG_POST_MSG("    Fix-One-Count: %s",
               CFG_print_strings_to_string(IP_NAMES, ", ").c_str());
  for (auto& iter : IP_NAMES) {
    BitAssembler_DDB_IP_00* const ip = find_ip(ddb.layout_ips, iter);
    CFG_ASSERT(ddb.ip_infos.find(ip->name) == ddb.ip_infos.end());
    assign_ip_info(ddb.ip_infos, ip, iter, distribution_ips);
  }
  std::map<std::string, std::vector<std::string>> IP_GROUPS = {
      {"sb", {"sb_0__1_", CFG_print("sb_%d__1_", ddb.max_logical_col)}}};
  for (auto& iter : IP_GROUPS) {
    CFG_POST_MSG("    Follow-Column: %s",
                 CFG_print_strings_to_string(iter.second, ", ").c_str())
    for (auto& iter_iter : iter.second) {
      CFG_ASSERT(ddb.ip_infos.find(iter_iter) == ddb.ip_infos.end());
      BitAssembler_DDB_IP_00* const ip = find_ip(ddb.layout_ips, iter_iter);
      for (uint32_t r = 0; r < ddb.row_size; r++) {
        BitAssembler_DDB_IP_00* temp = ddb.layout_ips[ip->col][r];
        if (temp != nullptr && temp->type == iter.first &&
            temp->name.size() == 0) {
          assign_ip_info(ddb.ip_infos, temp, iter_iter, distribution_ips);
        }
      }
    }
  }
  IP_GROUPS = {
      {"sb", {"sb_1__0_", CFG_print("sb_1__%d_", ddb.max_logical_row)}}};
  for (auto& iter : IP_GROUPS) {
    CFG_POST_MSG("    Follow-Row: %s",
                 CFG_print_strings_to_string(iter.second, ", ").c_str())
    for (auto& iter_iter : iter.second) {
      CFG_ASSERT(ddb.ip_infos.find(iter_iter) == ddb.ip_infos.end());
      BitAssembler_DDB_IP_00* const ip = find_ip(ddb.layout_ips, iter_iter);
      for (uint32_t c = 0; c < ddb.col_size; c++) {
        BitAssembler_DDB_IP_00* temp = ddb.layout_ips[c][ip->row];
        if (temp != nullptr && temp->type == iter.first &&
            temp->name.size() == 0) {
          assign_ip_info(ddb.ip_infos, temp, iter_iter, distribution_ips);
        }
      }
    }
  }
  IP_GROUPS = {{"sb", {"sb_1__1_", "sb_1__2_", "sb_2__2_"}}};
  for (auto& iter : IP_GROUPS) {
    CFG_POST_MSG("    Size-Macthing: %s",
                 CFG_print_strings_to_string(iter.second, ", ").c_str());
    // Make sure all type has unique bits
    std::vector<uint32_t> type_bits;
    for (auto& iter_iter : iter.second) {
      CFG_ASSERT(ddb.ip_infos.find(iter_iter) == ddb.ip_infos.end());
      CFG_ASSERT(
          CFG_find_u32_in_vector(type_bits, distribution_ips[iter_iter]) == -1);
      type_bits.push_back(distribution_ips[iter_iter]);
    }
    for (auto& iter_iter : iter.second) {
      for (uint32_t c = 0; c < ddb.col_size; c++) {
        for (uint32_t r = 0; r < ddb.row_size; r++) {
          if (ddb.layout_ips[c][r] != nullptr &&
              ddb.layout_ips[c][r]->type == iter.first &&
              ddb.layout_ips[c][r]->name.size() == 0 &&
              distribution_ips[ddb.layout_ips[c][r]->alias] ==
                  distribution_ips[iter_iter]) {
            assign_ip_info(ddb.ip_infos, ddb.layout_ips[c][r], iter_iter,
                           distribution_ips);
          }
        }
      }
    }
  }
  for (uint32_t c = 0; c < ddb.col_size; c++) {
    for (uint32_t r = 0; r < ddb.row_size; r++) {
      CFG_ASSERT(ddb.layout_ips[c][r] == nullptr ||
                 ddb.layout_ips[c][r]->name.size() > 0);
    }
  }
  // Assign IP value
  CFG_POST_MSG("Assign IP value");
  for (auto& iter : ddb.ip_infos) {
    CFG_ASSERT(iter.second->count == 0);
  }
  for (uint32_t c = 0; c < ddb.col_size; c++) {
    for (uint32_t r = 0; r < ddb.row_size; r++) {
      if (ddb.layout_ips[c][r] != nullptr) {
        CFG_ASSERT(ddb.layout_ips[c][r]->id == uint32_t(-1));
        CFG_ASSERT(ddb.layout_ips[c][r]->value == uint32_t(-1));
        ddb.layout_ips[c][r]->value =
            ddb.ip_infos[ddb.layout_ips[c][r]->name]->count;
        ddb.ip_infos[ddb.layout_ips[c][r]->name]->count++;
      }
    }
  }
  // Assign IP ID
  CFG_POST_MSG("Assign IP ID");
  auto assign_ip_id = [](std::vector<BitAssembler_DDB_IP_00*>& region_ips,
                         BitAssembler_DDB_IP_00* const ip) {
    CFG_ASSERT(ip != nullptr);
    CFG_ASSERT(ip->id == uint32_t(-1));
    ip->id = (uint32_t)(region_ips.size());
    region_ips.push_back(ip);
  };
  CFG_POST_MSG("  Sequence 1: From last column at last row");
  for (uint32_t c = 0, rc = ddb.col_size - 1; c < ddb.col_size; c++, rc--) {
    if (ddb.layout_ips[rc][ddb.row_size - 1] != nullptr &&
        ddb.layout_ips[rc][ddb.row_size - 1]->id == uint32_t(-1)) {
      assign_ip_id(ddb.region_ips, ddb.layout_ips[rc][ddb.row_size - 1]);
    }
  }
  CFG_POST_MSG("  Sequence 2: From last row at column #1");
  for (uint32_t r = 0, rr = ddb.row_size - 1; r < ddb.row_size; r++, rr--) {
    if (ddb.layout_ips[1][rr] != nullptr &&
        ddb.layout_ips[1][rr]->id == uint32_t(-1)) {
      assign_ip_id(ddb.region_ips, ddb.layout_ips[1][rr]);
    }
  }
  CFG_POST_MSG("  Sequence 3: Z pattern");
  bool forward = true;
  uint32_t z_col = 2;
  uint32_t z_row = 1;
  while (1) {
    CFG_ASSERT(z_col > 0);
    CFG_ASSERT((z_col + 1) < ddb.col_size);
    CFG_ASSERT(z_row > 0);
    CFG_ASSERT((z_row + 1) < ddb.row_size);
    std::vector<std::vector<uint32_t>> coords = {{z_col + 1, z_row},
                                                 {z_col, z_row},
                                                 {z_col + 1, z_row + 1},
                                                 {z_col, z_row + 1}};
    for (auto& iter : coords) {
      if (ddb.layout_ips[iter[0]][iter[1]] != nullptr &&
          ddb.layout_ips[iter[0]][iter[1]]->id == uint32_t(-1)) {
        assign_ip_id(ddb.region_ips, ddb.layout_ips[iter[0]][iter[1]]);
      }
    }
    if (forward) {
      z_col += 2;
      if (z_col == ddb.col_size) {
        z_col -= 2;
        z_row += 2;
        forward = false;
        if (z_row == ddb.row_size - 1) {
          break;
        }
      }
    } else {
      z_col -= 2;
      if (z_col == 0) {
        z_col += 2;
        z_row += 2;
        forward = true;
        if (z_row == ddb.row_size - 1) {
          break;
        }
      }
    }
  }
  for (uint32_t c = 0; c < ddb.col_size; c++) {
    for (uint32_t r = 0; r < ddb.row_size; r++) {
      CFG_ASSERT(ddb.layout_ips[c][r] == nullptr ||
                 (ddb.layout_ips[c][r]->id != uint32_t(-1) &&
                  ddb.layout_ips[c][r]->value != uint32_t(-1)));
    }
  }
  // BL and WL
  CFG_POST_MSG("Generate BL and WL information");
  ddb.update_bl_wl();
  ddb.finalize_configuration_bits();
  // IP Paths
  uint32_t path_index = 0;
  for (auto& iter : ddb.region_ips) {
    if (ddb.ip_infos[iter->name]->paths.size() == 0) {
      for (uint32_t i = 0; i < ddb.ip_infos[iter->name]->bits; i++) {
        ddb.ip_infos[iter->name]->paths.push_back(paths[path_index + i]);
      }
    }
    for (uint32_t i = 0; i < ddb.ip_infos[iter->name]->bits;
         i++, path_index++) {
      CFG_ASSERT(ddb.ip_infos[iter->name]->paths[i] == paths[path_index])
    }
  }
  CFG_ASSERT((size_t)(path_index) == paths.size());
  // Database
  std::vector<std::string> ip_sequences;
  for (auto& iter : ddb.ip_infos) {
    CFG_ASSERT(CFG_find_string_in_vector(ip_sequences, iter.first) == -1);
    ip_sequences.push_back(iter.first);
  }
  std::sort(ip_sequences.begin(), ip_sequences.end());
  CFGObject_DDB_00 obj;
  obj.write_str("device", device);
  obj.write_strs("types", ip_sequences);
  for (auto& iter : ip_sequences) {
    obj.append_u32("bits", ddb.ip_infos[iter]->bits);
    obj.append_strs("paths", ddb.ip_infos[iter]->paths);
  }
  int ip_index = 0;
  for (auto& iter : ddb.region_ips) {
    ip_index = CFG_find_string_in_vector(ip_sequences, iter->name);
    CFG_ASSERT(ip_index >= 0);
    obj.append_u32("ips", (uint32_t)(ip_index));
    obj.append_u32("ips", iter->value);
    obj.append_u32("ips", iter->col);
    obj.append_u32("ips", iter->row);
  }
  obj.write_u32s("bls", ddb.bls);
  obj.write_u32s("wls", ddb.wls);
  obj.write(output_ddb);
}

static BitAssembler_DDB_00* CFG_ddb_read_database(const CFGObject_DDB_00* obj) {
  CFG_POST_MSG("Construct database");
  CFG_ASSERT(obj != nullptr);
  CFG_ASSERT(obj->get_name() == "DDB_00");
  CFG_ASSERT(obj->types.size());
  CFG_ASSERT(obj->types.size() == obj->bits.size());
  CFG_ASSERT((obj->ips.size() % 4) == 0);
  BitAssembler_DDB_00* ddb = CFG_MEM_NEW(BitAssembler_DDB_00);
  uint32_t path_index = 0;
  for (auto& iter : obj->types) {
    CFG_ASSERT(ddb->ip_infos.find(iter) == ddb->ip_infos.end());
    ddb->ip_infos[iter] = CFG_MEM_NEW(BitAssembler_DDB_IP_INFO_00,
                                      obj->bits[ddb->ip_infos.size()]);
    for (uint32_t i = 0; i < ddb->ip_infos[iter]->bits; i++, path_index++) {
      ddb->ip_infos[iter]->paths.push_back(obj->paths[path_index]);
    }
  }
  CFG_ASSERT(path_index == obj->paths.size());
  BitAssembler_DDB_IP_00* ip = nullptr;
  for (uint32_t i = 0; i < obj->ips.size(); i += 4) {
    CFG_ASSERT(obj->ips[i] < (uint32_t)(obj->types.size()));
    ip = CFG_MEM_NEW(BitAssembler_DDB_IP_00, obj->types[obj->ips[i]],
                     (uint32_t)(ddb->region_ips.size()), obj->ips[i + 1],
                     obj->ips[i + 2], obj->ips[i + 3]);
    while (ddb->layout_ips.size() <= ip->col) {
      ddb->layout_ips.push_back({});
    }
    while (ddb->layout_ips[ip->col].size() <= ip->row) {
      ddb->layout_ips[ip->col].push_back(nullptr);
    }
    CFG_ASSERT(ddb->layout_ips[ip->col][ip->row] == nullptr);
    ddb->layout_ips[ip->col][ip->row] = ip;
    ddb->region_ips.push_back(ip);
    if (ip->col > ddb->col_size) {
      ddb->col_size = ip->col;
    }
    if (ip->row > ddb->row_size) {
      ddb->row_size = ip->row;
    }
  }
  ddb->col_size++;
  ddb->row_size++;
  CFG_ASSERT((uint32_t)(obj->bls.size()) == ddb->col_size);
  CFG_ASSERT((uint32_t)(obj->wls.size()) == ddb->row_size);
  ddb->finalize_layout_ips();
  ddb->bls = obj->bls;
  ddb->wls = obj->wls;
  ddb->finalize_bl_wl();
  ddb->finalize_configuration_bits();
  return ddb;
}

void CFG_ddb_gen_bitstream_00(const CFGObject_DDB_00* obj,
                              const std::string& input_bit,
                              const std::string& output_bit, bool reverse) {
  BitAssembler_DDB_00* ddb = CFG_ddb_read_database(obj);
  CFG_POST_MSG("Read bitstream bit file");
  std::vector<uint8_t> ccff;
  BitAssembler_MGR::get_one_region_ccff_fcb(input_bit, ccff);
  CFG_ASSERT((size_t)(ddb->configuration_bits) == ccff.size());
  CFG_POST_MSG("Generate QL Memory Bank Bitstream");
  ddb->create_blwls();
  uint32_t bl = 0;
  uint32_t wl = 0;
  uint32_t byte_size = (ddb->bl + 7) / 8;
  std::vector<std::vector<uint8_t>> data;
  std::vector<std::vector<uint8_t>> mask;
  data.resize(ddb->wl);
  mask.resize(ddb->wl);
  for (uint32_t id = 0, bit = ddb->configuration_bits - 1;
       id < ddb->configuration_bits; id++, bit--) {
    bl = ddb->blwls[id].first;
    wl = ddb->blwls[id].second;
    if (data[wl].size() == 0) {
      CFG_ASSERT(mask[wl].size() == 0);
      data[wl].resize(byte_size);
      mask[wl].resize(byte_size);
      memset(&data[wl][0], 0, byte_size);
      memset(&mask[wl][0], 0xFF, byte_size);
    }
    if (ccff[bit]) {
      data[wl][bl >> 3] |= (uint8_t)(1 << (bl & 7));
    }
    mask[wl][bl >> 3] &= (uint8_t)(~(1 << (bl & 7)));
  }
  std::ofstream bit;
  bit.open(output_bit.c_str());
  CFG_ASSERT(bit.good());
  bit << "// Fabric bitstream\n";
  bit << "// Version:\n";
  bit << "// Date:\n";
  bit << "// Bitstream length: " << std::to_string(ddb->wl) << "\n";
  bit << CFG_print(
             "// Bitstream width (LSB -> MSB): <bl_address %d bits><wl_address "
             "%d bits>\n",
             ddb->bl, ddb->wl)
             .c_str();
  bit << "// Protocol: QL Memory Bank\n";
  bit << "// DDB: 00\n";
  for (uint32_t wl = 0, rwl = ddb->wl - 1, cwl = 0; wl < ddb->wl;
       wl++, rwl--, cwl++) {
    if (reverse) {
      cwl = rwl;
    }
    for (uint32_t bl = 0; bl < ddb->bl; bl++) {
      if (mask[cwl][bl >> 3] & (1 << (bl & 7))) {
        bit << "x";
      } else if (data[cwl][bl >> 3] & (1 << (bl & 7))) {
        bit << "1";
      } else {
        bit << "0";
      }
    }
    for (uint32_t wl = 0; wl < ddb->wl; wl++) {
      if (wl == cwl) {
        bit << "1";
      } else {
        bit << "0";
      }
    }
    bit << "\n";
  }
  CFG_MEM_DELETE(ddb);
}

void CFG_ddb_gen_fabric_bitstream_xml_00(const CFGObject_DDB_00* obj,
                                         const std::string& protocol,
                                         const std::string& input_bit,
                                         const std::string& output_xml,
                                         bool reverse) {
  BitAssembler_DDB_00* ddb = CFG_ddb_read_database(obj);
  CFG_POST_MSG("Read bitstream bit file");
  std::vector<uint8_t> ccff;
  BitAssembler_MGR::get_one_region_ccff_fcb(input_bit, ccff);
  CFG_ASSERT((size_t)(ddb->configuration_bits) == ccff.size());
  CFG_POST_MSG("Generate %s fabric bitstream XML", protocol.c_str());
  std::ofstream xml;
  xml.open(output_xml.c_str());
  CFG_ASSERT(xml.good());
  size_t path_index = 0;
  if (protocol == "ccff") {
    for (auto& iter : ddb->ip_infos) {
      for (auto& iter_iter : iter.second->paths) {
        path_index = iter_iter.find("RS_LATCH");
        while (path_index != std::string::npos) {
          iter_iter.replace(path_index, 8, "RS_CCFF");
          path_index = iter_iter.find("RS_LATCH");
        }
      }
    }
  } else {
    ddb->create_blwls();
    for (auto& iter : ddb->ip_infos) {
      for (auto& iter_iter : iter.second->paths) {
        path_index = iter_iter.find("RS_CCFF");
        while (path_index != std::string::npos) {
          iter_iter.replace(path_index, 7, "RS_LATCH");
          path_index = iter_iter.find("RS_CCFF");
        }
      }
    }
  }
  xml << "<fabric_bitstream>\n";
  xml << "\t<region id=\"0\">\n";
  uint32_t id = 0;
  uint32_t bit = ddb->configuration_bits - 1;
  uint32_t bl = 0;
  uint32_t wl = 0;
  std::vector<char> bl_addr;
  std::vector<char> wl_addr;
  bl_addr.resize(ddb->bl + 1);
  wl_addr.resize(ddb->wl + 1);
  memset(&bl_addr[0], 'x', ddb->bl);
  memset(&wl_addr[0], '0', ddb->wl);
  bl_addr[ddb->bl] = 0;
  wl_addr[ddb->wl] = 0;
  if (reverse) {
    id = bit;
    bit = 0;
  }
  for (size_t i = 0, ri = ddb->region_ips.size() - 1;
       i < ddb->region_ips.size(); i++, ri--) {
    BitAssembler_DDB_IP_00* ip =
        reverse ? ddb->region_ips[ri] : ddb->region_ips[i];
    BitAssembler_DDB_IP_INFO_00* ip_info = ddb->ip_infos[ip->name];
    for (size_t j = 0, rj = ip_info->bits - 1; j < ip_info->bits; j++, rj--) {
      xml << "\t\t<bit id=\"";
      xml << std::to_string(id).c_str();
      xml << "\" value=\"";
      xml << std::to_string(ccff[bit]).c_str();
      xml << "\" path=\"fpga_top.";
      xml << ip->alias.c_str();
      xml << ".";
      if (reverse) {
        xml << ip_info->paths[rj].c_str();
      } else {
        xml << ip_info->paths[j].c_str();
      }
      xml << "\">\n";
      if (protocol == "latch") {
        bl = ddb->blwls[id].first;
        wl = ddb->blwls[id].second;
        // BL
        xml << "\t\t\t<bl address=\"";
        bl_addr[bl] = '1';
        xml << &bl_addr[0];
        bl_addr[bl] = 'x';
        xml << "\"/>\n";
        // WL
        xml << "\t\t\t<wl address=\"";
        wl_addr[wl] = '1';
        xml << &wl_addr[0];
        wl_addr[wl] = '0';
        xml << "\"/>\n";
      }
      xml << "\t\t</bit>\n";
      if (reverse) {
        id--;
        bit++;
      } else {
        id++;
        bit--;
      }
    }
    std::string msg =
        CFG_print("  Percentage: %.1f%\r",
                  (float)((i + 1) * 100) / (float)(ddb->region_ips.size()));
    CFG_post_msg(msg, "INFO: ", false);
  }
  xml << " </region>\n";
  xml << "</fabric_bitstream>\n";
  xml.close();
  CFG_MEM_DELETE(ddb);
}