#include "BitGen_analyzer.h"

BitGen_ANALYZER::BitGen_ANALYZER(const std::string& filepath,
                                 std::ofstream* file,
                                 std::vector<uint8_t>* aes_key)
    : m_filepath(filepath), m_file(file), m_aes_key(aes_key) {
  CFG_ASSERT(m_file != nullptr);
  CFG_ASSERT(m_file->is_open());
  CFG_ASSERT(m_file->good());
  CFG_ASSERT(m_aes_key == nullptr || m_aes_key->size() == 16 ||
             m_aes_key->size() == 32);
}

BitGen_ANALYZER::~BitGen_ANALYZER() {
  memset(m_decompressed_data, 0, sizeof(m_decompressed_data));
}

std::string BitGen_ANALYZER::get_null_terminate_string(const uint8_t* data,
                                                       size_t max_size) {
  size_t index = 0;
  return CFG_get_string_from_bytes(data, max_size, index, -1, -1, max_size);
}

template <typename T>
void BitGen_ANALYZER::get_data(const uint8_t* data, T& value) {
  value = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    value |= (T)((uint64_t(data[i]) & 0xFF) << (i * 8));
  }
}

uint32_t BitGen_ANALYZER::get_u32(const uint8_t* data) {
  uint32_t temp = 0;
  get_data(data, temp);
  return temp;
}

uint64_t BitGen_ANALYZER::get_u64(const uint8_t* data) {
  uint64_t temp = 0;
  get_data(data, temp);
  return temp;
}

void BitGen_ANALYZER::update_iv(uint8_t* iv) {
  uint32_t counter = get_u32(iv);
  counter++;
  memcpy(iv, (void*)(&counter), sizeof(counter));
}

std::string BitGen_ANALYZER::print_repeat_word_line(std::string word,
                                                    uint32_t repeat) {
  CFG_ASSERT(word.size());
  CFG_ASSERT(repeat);
  std::string line = "";
  for (uint32_t i = 0; i < repeat; i++) {
    line += word;
  }
  return line;
}

bool BitGen_ANALYZER::parse_bop(const uint8_t* data, size_t size,
                                uint32_t index, const size_t offset) {
  CFG_ASSERT(size);
  CFG_ASSERT((size % BitGen_BITSTREAM_BLOCK_SIZE) == 0);
  CFG_ASSERT(size < (uint64_t)(0x100000000));
  CFG_ASSERT((offset % BitGen_BITSTREAM_BLOCK_SIZE) == 0);
  m_current_bop_data = data;
  m_current_bop_size = size;
  m_current_bop_index = index;
  m_current_bop_offset = offset;
  std::string space = "    ";
  (*m_file) << space.c_str() << "BOP #" << m_current_bop_index << "\n";
  space += "  ";
  // Header
  m_status.reset();
  m_header.reset();
  m_integrity.reset();
  uint8_t unobscured_data[16];
  parse_bop_header(space, unobscured_data);
  // Action
  uint32_t action_index = 0;
  std::vector<std::string> action_msgs;
  std::vector<BitGen_ANALYZER_ACTION*> actions;
  if (m_header.action_count == 0) {
    post_error(space, "Invalid action count");
    m_status.status = false;
    m_status.action_status = false;
  } else if (m_status.compression_status && m_status.encryption_status) {
    parse_action((const uint32_t*)(&m_current_bop_data[0xC8]), 78, action_index,
                 action_msgs, actions);
    if (!m_status.action_status) {
      post_error(space, "Fail to parse action");
    }
  }
  // Authentication
  authenticate(space);
  // Challenge
  std::vector<std::string> challenge_msgs;
  challenge(space, challenge_msgs);
  // Print header
  space += "  ";
  print_header(space, unobscured_data, action_msgs, challenge_msgs);
  if (m_status.status) {
    space.pop_back();
    space.pop_back();
    m_current_bop_data_index = BitGen_BITSTREAM_BLOCK_SIZE;
    uint32_t payload_index = 0;
    for (uint32_t i = 0; i < m_header.action_count && m_status.status; i++) {
      CFG_ASSERT(action_msgs.size() == 0);
      if (actions.size() == 0) {
        (*m_file) << space.c_str()
                  << "Info: Running out of action. Read action block\n";
        size_t action_addr = get_next_block(space + "  ", "action");
        if (action_addr == 0) {
          m_status.status = false;
        } else {
          parse_action((const uint32_t*)(&m_current_bop_data[action_addr]),
                       BitGen_BITSTREAM_BLOCK_SIZE / 4, action_index,
                       action_msgs, actions);
          if (!m_status.action_status) {
            post_error(space, "Fail to parse action");
          }
          print_action_block(space + "  ", &m_current_bop_data[action_addr],
                             action_addr, action_msgs);
          CFG_ASSERT(action_msgs.size() == 0);
        }
      }
      if (actions.size()) {
        CFG_ASSERT(actions.front() != nullptr);
        if (actions.front()->payload_size) {
          uint32_t payload_block_count =
              (uint32_t)((actions.front()->payload_size +
                          BitGen_BITSTREAM_BLOCK_SIZE - 1) /
                         BitGen_BITSTREAM_BLOCK_SIZE);
          (*m_file)
              << space.c_str()
              << CFG_print(
                     "Info: Firmware execute Action (0x%03X) that need %d "
                     "Bytes of payload (%d 2K Block(s))\n",
                     actions.front()->cmd, actions.front()->payload_size,
                     payload_block_count)
                     .c_str();
          uint8_t bop_iv[16];
          memcpy(bop_iv, m_header.iv, sizeof(bop_iv));
          m_decompress_engine.reset();
          m_status.decompression_status = BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
          m_decompressed_data_offset = 0;
          std::string binfilepath =
              CFG_print("%s.bop%d.payload%d.bin",
                        m_filepath.substr(0, m_filepath.size() - 4).c_str(),
                        m_current_bop_index, payload_index);
          std::ofstream binfile(binfilepath.c_str(),
                                std::ios::out | std::ios::binary);
          for (uint32_t j = 0, k = payload_block_count - 1;
               j < payload_block_count && m_status.status; j++, k--) {
            (*m_file) << space.c_str() << "  "
                      << CFG_print("Get payload #%d", j).c_str() << "\n";
            size_t payload_addr = get_next_block(space + "    ", "payload");
            if (payload_addr == 0) {
              m_status.status = false;
            } else {
              size_t payload_size = actions.front()->payload_size -
                                    (j * BitGen_BITSTREAM_BLOCK_SIZE);
              if (payload_size > BitGen_BITSTREAM_BLOCK_SIZE) {
                payload_size = BitGen_BITSTREAM_BLOCK_SIZE;
              }
              parse_payload(
                  space + "    ", &data[payload_addr], payload_size,
                  payload_addr, actions.front()->compression,
                  actions.front()->has_iv,
                  actions.front()->has_iv ? actions.front()->iv : bop_iv,
                  k == 0, binfile);
            }
          }
          payload_index++;
          binfile.close();
          if (m_aes_key != nullptr && !actions.front()->has_iv) {
            update_iv(m_header.iv);
          }
          if (m_status.status) {
            std::vector<uint8_t> bindata;
            if (m_header.encryption == "none" || m_aes_key != nullptr) {
              CFG_read_binary_file(binfilepath, bindata);
              if (bindata.size() == 0) {
                post_warning(space, CFG_print("Impossible %s is content empty",
                                              binfilepath.c_str()));
              } else if (bindata.size() % 4) {
                post_warning(
                    space,
                    CFG_print(
                        "Impossible %s file size is not multiple of 4 Bytes",
                        binfilepath.c_str()));
              } else {
                if (actions.front()->has_original_payload_size) {
                  if (actions.front()->original_payload_size ==
                      (uint32_t)(bindata.size())) {
                    (*m_file) << space.c_str()
                              << "Info: Successfully verify original payload "
                                 "size for "
                              << binfilepath.c_str() << "\n";
                  } else {
                    post_error(
                        space,
                        CFG_print("Fail to verify original payload size for %s",
                                  binfilepath.c_str()));
                    m_status.status = false;
                    m_status.checksum_status = false;
                  }
                } else {
                  (*m_file) << space.c_str()
                            << "Info: Do not check original payload size for "
                            << binfilepath.c_str() << " \n";
                }
                if (actions.front()->has_checksum) {
                  (*m_file) << space.c_str() << "Info: Verify checksum of "
                            << binfilepath.c_str() << "\n";
                  uint8_t checksum_enum =
                      BitGen_PACKER::get_feature_u8_enum(m_header.checksum);
                  uint32_t checksum_size = 0;
                  uint64_t u64 = BitGen_PACKER::calc_checksum(
                      bindata, checksum_enum, checksum_size);
                  CFG_ASSERT(checksum_size == 4);
                  uint32_t payload_checksum = (uint32_t)(u64);
                  if (actions.front()->checksum_value == payload_checksum) {
                    (*m_file) << space.c_str()
                              << "Info: Successfully verify checksum for "
                              << binfilepath.c_str() << "\n";
                  } else {
                    post_error(space, CFG_print("Fail to veriy checksum for %s",
                                                binfilepath.c_str()));
                    m_status.status = false;
                    m_status.checksum_status = false;
                  }
                } else {
                  (*m_file)
                      << space.c_str() << "Info: Do not check checksum for "
                      << binfilepath.c_str() << " \n";
                }
              }
            }
          }
        } else {
          (*m_file)
              << space.c_str()
              << CFG_print(
                     "Info: Firmware execute Action (0x%03X) without payload\n",
                     actions.front()->cmd)
                     .c_str();
        }
        CFG_MEM_DELETE(actions.front());
        actions.erase(actions.begin());
      }
    }
  }
  while (actions.size()) {
    CFG_MEM_DELETE(actions.back());
    actions.pop_back();
  }
  return m_status.status;
}

void BitGen_ANALYZER::parse_bop_header(std::string space,
                                       uint8_t (&unobscured_data)[16]) {
  m_header.identifier = get_null_terminate_string(m_current_bop_data, 4);
  m_header.version = get_u32(&m_current_bop_data[0x4]);
  m_header.size = (size_t)(get_u64(&m_current_bop_data[0x8]));
  CFG_ASSERT(m_header.size == m_current_bop_size);
  m_header.tool = get_null_terminate_string(&m_current_bop_data[0x10], 32);
  m_header.opn = get_null_terminate_string(&m_current_bop_data[0x30], 16);
  m_header.jtag_id = get_u32(&m_current_bop_data[0x40]);
  m_header.jtag_mask = get_u32(&m_current_bop_data[0x44]);
  memcpy(unobscured_data, &m_current_bop_data[0x50], sizeof(unobscured_data));
  BitGen_PACKER::obscure(unobscured_data, &m_current_bop_data[0x200]);
  uint32_t unobscured_expected_crc32 = CFG_crc32(unobscured_data, 12);
  uint32_t unobscured_crc32 = get_u32(&unobscured_data[12]);
  m_header.chipid = "unknown";
  if (unobscured_expected_crc32 == unobscured_crc32) {
    m_header.chipid = CFG_print("0x%02X", unobscured_data[0]);
  } else {
    post_error(space, "Obscured data CRC fail");
    m_status.status = false;
  }
  m_header.checksum = BitGen_PACKER::get_feature_enum_string(
      m_current_bop_data[0x60], {"flecther32"}, m_status.checksum_status);
  if (m_header.checksum == "unknown") {
    post_error(space, "Unknown checksum");
    m_status.status = false;
  }
  m_header.compression = BitGen_PACKER::get_feature_enum_string(
      m_current_bop_data[0x61], {"dcmp0"}, m_status.compression_status, true);
  if (m_header.compression == "unknown") {
    post_error(space, "Unknown compression");
    m_status.status = false;
  }
  m_header.integrity = BitGen_PACKER::get_feature_enum_string(
      m_current_bop_data[0x62], {"sha256", "sha384", "sha512"},
      m_status.integrity_status);
  if (m_header.integrity == "unknown") {
    post_error(space, "Unknown integrity");
    m_status.status = false;
  }
  m_header.encryption = BitGen_PACKER::get_feature_enum_string(
      m_current_bop_data[0x80], {"ctr128", "ctr256"},
      m_status.encryption_status, true);
  if (m_header.encryption == "unknown") {
    post_error(space, "Unknown encryption");
    m_status.status = false;
  }
  m_header.authentication = BitGen_PACKER::get_feature_enum_string(
      m_current_bop_data[0x81], {"ecdsa256", "rsa2048"},
      m_status.authentication_status, true);
  if (m_header.authentication == "unknown") {
    post_error(space, "Unknown authentication");
    m_status.status = false;
  }
  m_header.action_version = get_u32(&m_current_bop_data[0xC0]);
  m_header.action_count = get_u32(&m_current_bop_data[0xC4]);
  // Integrity
  if (m_status.integrity_status && m_header.integrity != "unknown") {
    int temp = CFG_find_string_in_vector({"sha256", "sha384", "sha512"},
                                         m_header.integrity);
    CFG_ASSERT(temp >= 0);
    m_integrity.size = 32 + (16 * temp);
    m_integrity.remaining_size = m_integrity.size;
    m_integrity.addr = const_cast<uint8_t*>(&m_current_bop_data[0x200]);
  }
  if (m_status.encryption_status && m_header.encryption != "unknown") {
    memcpy(m_header.iv, &m_current_bop_data[0x280], sizeof(m_header.iv));
  }
  m_header.is_last_bop_block = (m_current_bop_data[0x780] & 1) != 0;
  m_header.end_size = (size_t)(get_u64(&m_current_bop_data[0x788]));
  m_header.crc32 = get_u32(&m_current_bop_data[0x7FC]);
  uint32_t crc32 = CFG_crc32(m_current_bop_data, 0x7FC);
  if (crc32 != m_header.crc32) {
    post_error(space, CFG_print("Invalid CRC - expect 0x%08X but found 0x%08X",
                                crc32, m_header.crc32));
    m_status.status = false;
  }
}

void BitGen_ANALYZER::parse_action(
    const uint32_t* data, size_t size, uint32_t& action_index,
    std::vector<std::string>& msgs,
    std::vector<BitGen_ANALYZER_ACTION*>& actions) {
  CFG_ASSERT(m_status.action_status);
  CFG_ASSERT(m_header.action_count);
  CFG_ASSERT(m_header.action_count);
  CFG_ASSERT(action_index < m_header.action_count);
  uint8_t tracker = 0;
  size_t action_size = 0;
  for (size_t i = 0; i < size && m_status.action_status; i++) {
    switch (tracker) {
      case 0:
        if ((data[i] & 0xFFF) == 0) {
          if (data[i] == 0) {
            tracker = 0xFF;
            if (action_index < m_header.action_count) {
              msgs.push_back(" No more Action for now (more in Action Block)");
            } else {
              msgs.push_back(" No more Action");
            }
          } else {
            push_error(msgs, "Invalid action termination");
            m_status.action_status = false;
          }
        } else if (action_index == m_header.action_count) {
          push_error(msgs, CFG_print("Exceeding maximum action count (%d)",
                                     m_header.action_count));
          m_status.action_status = false;
        } else {
          action_size = (size_t)(data[i] >> 16);
          if (action_size == 0) {
            push_error(msgs, "Invalid ZERO size action");
            m_status.action_status = false;
          } else if (action_size % 4) {
            push_error(msgs, "Invalid none-4Byte-aligned size action");
            m_status.action_status = false;
          } else if (action_size < 8) {
            push_error(msgs, "Invalid none-minimum-8Byte size action");
            m_status.action_status = false;
          } else if ((i + (action_size / 4)) > size) {
            push_error(msgs, "Invalid overflow size action");
            m_status.action_status = false;
          } else {
            BitGen_ANALYZER_ACTION* action =
                CFG_MEM_NEW(BitGen_ANALYZER_ACTION);
            action->cmd = data[i] & 0xFFF;
            action->has_checksum = (data[i] & 0x1000) != 0;
            action->compression = (data[i] & 0x2000) != 0;
            action->has_iv = (data[i] & 0x4000) != 0;
            action->has_original_payload_size = (data[i] & 0x8000) != 0;
            action->size = (uint16_t)(action_size);
            if (data[i + 1] == 0 && action->has_checksum) {
              push_error(
                  msgs, "Invalid Bit12 set in command but there is no payload");
              m_status.action_status = false;
            } else if (m_header.compression == "none" && action->compression) {
              push_error(msgs,
                         "Invalid Bit13 set in command but compression feature "
                         "is OFF");
              m_status.action_status = false;
            } else if (m_header.encryption == "none" && action->has_iv) {
              push_error(
                  msgs,
                  "Invalid Bit14 set in command but encryption feature is OFF");
              m_status.action_status = false;
            } else if (data[i + 1] == 0 && action->has_original_payload_size) {
              push_error(
                  msgs, "Invalid Bit15 set in command but there is no payload");
              m_status.action_status = false;
            } else {
              // Last step size must match up
              uint16_t minimum_size = 8;
              if (action->has_checksum) {
                minimum_size += 4;
              }
              if (action->has_iv) {
                action->iv_size = 16;
                minimum_size += 16;
              }
              if (action->has_original_payload_size) {
                minimum_size += 4;
              }
              if (minimum_size > action->size) {
                push_error(msgs, CFG_print("Invalid action size. Expected "
                                           "mimumum size is 0x%04X (%d) Bytes",
                                           minimum_size, minimum_size));
                m_status.action_status = false;
              } else {
                action->field_size = action->size - minimum_size;
                msgs.push_back(CFG_print(
                    " Action: 0x%03X (Checksum: %d, Force Disable "
                    "Compression: %d, IV: %d, Original Payload Size: %d}); "
                    "Size: 0x%04X (%d)",
                    action->cmd, action->has_checksum, action->compression,
                    action->has_iv, action->has_original_payload_size,
                    action->size, action->size));
                actions.push_back(action);
                tracker = 1;
                action_index++;
              }
            }
            if (!m_status.action_status) {
              CFG_MEM_DELETE(action);
            }
          }
        }
        break;
      case 1:
        // Retrieve payload size
        CFG_ASSERT(actions.size());
        CFG_ASSERT(actions.back() != nullptr);
        actions.back()->payload_size = data[i];
        if ((m_header.compression == "none" || actions.back()->compression) &&
            ((actions.back()->payload_size % 4) != 0)) {
          push_error(
              msgs,
              CFG_print("Compression is effectively turned off but payload "
                        "size is not multiple of 4 Byte(s) - 0x%08X (%d)",
                        actions.back()->payload_size,
                        actions.back()->payload_size));
        } else {
          msgs.push_back(CFG_print("   Payload Size: 0x%08X (%d)",
                                   actions.back()->payload_size,
                                   actions.back()->payload_size));
        }
        if (actions.back()->size == 8) {
          tracker = 0;
        } else {
          if (actions.back()->has_original_payload_size) {
            tracker = 2;
          } else if (actions.back()->has_checksum) {
            tracker = 3;
          } else {
            tracker = 4;
          }
        }
        break;
      case 2:
        // Retrieve original payload size
        CFG_ASSERT(actions.size());
        CFG_ASSERT(actions.back() != nullptr);
        actions.back()->original_payload_size = data[i];
        msgs.push_back(CFG_print("   Original Payload Size: 0x%08X", data[i]));
        if (actions.back()->size == 12) {
          tracker = 0;
        } else if (actions.back()->has_checksum) {
          tracker = 3;
        } else {
          tracker = 4;
        }
        break;
      case 3:
        CFG_ASSERT(actions.size());
        CFG_ASSERT(actions.back() != nullptr);
        actions.back()->checksum_value = data[i];
        msgs.push_back(CFG_print("   Checksum: 0x%08X", data[i]));
        if ((actions.back()->has_original_payload_size &&
             actions.back()->size == 16) ||
            (!actions.back()->has_original_payload_size &&
             actions.back()->size == 12)) {
          tracker = 0;
        } else {
          tracker = 4;
        }
        break;
      case 4:
        CFG_ASSERT(actions.size());
        CFG_ASSERT(actions.back() != nullptr);
        if (actions.back()->field_tracker < actions.back()->field_size) {
          msgs.push_back(CFG_print("   Field #%d: 0x%08X",
                                   actions.back()->field_tracker / 4, data[i]));
          actions.back()->field_tracker += 4;
        } else if (actions.back()->iv_tracker < actions.back()->iv_size) {
          msgs.push_back(CFG_print("   IV #%d: 0x%08X",
                                   actions.back()->iv_tracker / 4, data[i]));
          memcpy(&actions.back()->iv[actions.back()->iv_tracker],
                 (void*)(&data[i]), sizeof(data[i]));
          actions.back()->iv_tracker += 4;
        }
        if (actions.back()->field_tracker == actions.back()->field_size &&
            actions.back()->iv_tracker == actions.back()->iv_size) {
          tracker = 0;
        }
        break;
      case 0xFF:
        if (data[i] != 0) {
          push_error(msgs, "Impossible to have more action");
          m_status.action_status = false;
        } else {
          msgs.push_back("");
        }
        break;
      default:
        CFG_INTERNAL_ERROR("Bugs in decoding action");
        break;
    }
  }
  m_status.status &= m_status.action_status;
}

void BitGen_ANALYZER::print_action_block(
    std::string space, const uint8_t* data, size_t action_addr,
    std::vector<std::string>& action_msgs) {
  (*m_file) << space.c_str() << "Block: Action\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  (*m_file) << space.c_str() << "  Absolute | Offset   | Data     | Remark\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  for (size_t i = 0; i < BitGen_BITSTREAM_BLOCK_SIZE; i += 4) {
    uint32_t u32 = get_u32(&data[i]);
    (*m_file) << space.c_str()
              << CFG_print("  %08X | %08X | %08X |",
                           m_current_bop_offset + action_addr + i,
                           action_addr + i, u32)
                     .c_str();
    if (action_msgs.size()) {
      (*m_file) << action_msgs.front();
      action_msgs.erase(action_msgs.begin());
    }
    (*m_file) << "\n";
  }
}

void BitGen_ANALYZER::authenticate(std::string space) {
  if (m_status.authentication_status && m_header.authentication != "none") {
    CFG_ASSERT(m_header.authentication == "ecdsa256" ||
               m_header.authentication == "rsa2048");
    uint32_t signature_size = m_header.authentication == "ecdsa256" ? 64 : 256;
    std::string key_type =
        m_header.authentication == "ecdsa256" ? "prime256v1" : "rsa2048";
    uint32_t pub_key_size = m_header.authentication == "ecdsa256" ? 64 : 260;
    m_status.authentication_status = CFGOpenSSL::authenticate_message(
        m_current_bop_data, 0x680, &m_current_bop_data[0x680], signature_size,
        key_type, &m_current_bop_data[0x570], pub_key_size);
    if (m_status.authentication_status) {
      (*m_file) << space.c_str() << "Info: Successfully authenticate data\n";
    } else {
      post_error(space, "Fail to authenticate data");
    }
    m_status.status &= m_status.authentication_status;
  }
}

void BitGen_ANALYZER::challenge(std::string space,
                                std::vector<std::string>& msgs) {
  if (m_status.encryption_status && m_header.encryption != "none") {
    CFG_ASSERT(m_header.encryption == "ctr128" ||
               m_header.encryption == "ctr256");
    if (m_aes_key != nullptr) {
      if ((m_header.encryption == "ctr128" && m_aes_key->size() == 16) ||
          (m_header.encryption == "ctr256" && m_aes_key->size() == 32)) {
        uint8_t plain_data[64] = {0};
        CFGOpenSSL::ctr_decrypt(&m_current_bop_data[0x240], plain_data,
                                sizeof(plain_data), m_aes_key->data(),
                                m_aes_key->size(), m_header.iv,
                                sizeof(m_header.iv));
        update_iv(m_header.iv);
        uint32_t challenge_crc32 =
            get_u32(&plain_data[sizeof(plain_data) - sizeof(challenge_crc32)]);
        uint32_t challenge_expected_crc32 = CFG_crc32(
            plain_data, sizeof(plain_data) - sizeof(challenge_expected_crc32));
        if (challenge_crc32 == challenge_expected_crc32) {
          (*m_file) << space.c_str() << "Info: Successfully challenge data\n";
          for (uint32_t i = 0; i < 64; i += 4) {
            msgs.push_back(CFG_print("%08X", get_u32(&plain_data[i])));
          }
        } else {
          post_error(space,
                     "Fail to challenge data. All encryption debugging will be "
                     "turned OFF");
          m_status.encryption_status = false;
        }
        memset(plain_data, 0, sizeof(plain_data));
      } else {
        post_warning(space,
                     "Provided AES Key size does not match with encryption "
                     "feature. All encryption debugging will be turned OFF");
        m_aes_key = nullptr;
      }
      m_status.status &= m_status.encryption_status;
    } else {
      post_warning(space,
                   "AES key is not provided to challenge and decrypt data");
    }
  } else {
    m_aes_key = nullptr;
  }
}

void BitGen_ANALYZER::print_header(std::string space,
                                   uint8_t (&unobscured_data)[16],
                                   std::vector<std::string>& action_msgs,
                                   std::vector<std::string>& challenge_msgs) {
  (*m_file) << space.c_str() << "Block: Header\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  (*m_file) << space.c_str()
            << "  Absolute | Offset   | Data     | Decode   | Remark\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  std::string unobscured_data_string = print_repeat_word_line(" ", 8);
  for (uint32_t i = 0; i < BitGen_BITSTREAM_BLOCK_SIZE; i += 4) {
    uint32_t u32 = get_u32(&m_current_bop_data[i]);
    if (i >= 0x50 && i < 0x60) {
      unobscured_data_string =
          CFG_print("%08X", get_u32(&unobscured_data[i - 0x50]));
    } else if (i >= 0x240 && i < 0x280 && challenge_msgs.size()) {
      unobscured_data_string = challenge_msgs.front();
      challenge_msgs.erase(challenge_msgs.begin());
    } else if (i == 0x60 || i == 0x280) {
      unobscured_data_string = print_repeat_word_line(" ", 8);
    }
    (*m_file) << space.c_str()
              << CFG_print("  %08X | %08X | %08X | %s |",
                           m_current_bop_offset + i, i, u32,
                           unobscured_data_string.c_str())
                     .c_str();
    if (i >= 0xC8 && i < 0x200) {
      // Stupid MSVC does not support switch case range 0xC8 ... 0x1FF
      if (action_msgs.size()) {
        (*m_file) << action_msgs.front().c_str();
        action_msgs.erase(action_msgs.begin());
      }
    } else {
      switch (i) {
        case 0:
          (*m_file) << " Identifer: " << m_header.identifier.c_str();
          break;
        case 4:
          (*m_file) << " Version: "
                    << CFG_print("0x%08X", m_header.version).c_str();
          break;
        case 8:
          (*m_file) << " Size: "
                    << CFG_print("0x%016X (%ld)", m_header.size, m_header.size)
                           .c_str();
          break;
        case 0x10:
          (*m_file) << " Tool: " << m_header.tool.c_str();
          break;
        case 0x30:
          (*m_file) << " OPN: " << m_header.opn.c_str();
          break;
        case 0x40:
          (*m_file) << " JTAG ID: "
                    << CFG_print("0x%08X", m_header.jtag_id).c_str();
          break;
        case 0x44:
          (*m_file) << " JTAG Mask: "
                    << CFG_print("0x%08X", m_header.jtag_mask).c_str();
          break;
        case 0x48:
          (*m_file) << " Reserved";
          break;
        case 0x50:
          (*m_file)
              << " Obscured data: "
              << CFG_print("(ChipID: %s)", m_header.chipid.c_str()).c_str();
          break;
        case 0x60:
          (*m_file) << " Non-Security Features: "
                    << CFG_print(
                           "(Checksum: %s; Compression: %s; Integrity: %s)",
                           m_header.checksum.c_str(),
                           m_header.compression.c_str(),
                           m_header.integrity.c_str())
                           .c_str();
          break;
        case 0x80:
          (*m_file) << " Security Features: "
                    << CFG_print("(Encryption: %s; Authentication: %s)",
                                 m_header.encryption.c_str(),
                                 m_header.authentication.c_str())
                           .c_str();
          break;
        case 0xA0:
          (*m_file) << " Reserved";
          break;
        case 0xC0:
          (*m_file) << " Action Version: "
                    << CFG_print("0x%08X", m_header.action_version).c_str();
          break;
        case 0xC4:
          (*m_file) << " Action Count: " << m_header.action_count;
          break;
        /*
        case 0xC8 ... 0x1FF:
          if (action_msgs.size()) {
            (*m_file) << action_msgs.front().c_str();
            action_msgs.erase(action_msgs.begin());
          }
          break;
        */
        case 0x200:
          (*m_file) << " Hash";
          if (m_header.size > BitGen_BITSTREAM_BLOCK_SIZE) {
            (*m_file)
                << CFG_print(
                       " (Used to verify block at addresss of Offset 0x%08X "
                       "(Absoulte 0x%08X))",
                       BitGen_BITSTREAM_BLOCK_SIZE,
                       m_current_bop_offset + BitGen_BITSTREAM_BLOCK_SIZE)
                       .c_str();
          }
          break;
        case 0x240:
          (*m_file) << " Challenge Data";
          break;
        case 0x280:
          (*m_file) << " IV";
          break;
        case 0x290:
          (*m_file) << " Reserved";
          break;
        case 0x570:
          (*m_file) << " Public Key";
          break;
        case 0x680:
          (*m_file) << " Authentication Signature";
          break;
        case 0x780:
          (*m_file)
              << " Flags: "
              << CFG_print("Last BOP: %d", m_header.is_last_bop_block).c_str();
          break;
        case 0x788:
          (*m_file) << " End Size: "
                    << CFG_print("0x%016X (%ld)", m_header.end_size,
                                 m_header.end_size)
                           .c_str();
          break;
        case 0x790:
          (*m_file) << " Reserved";
          break;
        case 0x7FC:
          (*m_file) << " CRC: " << CFG_print("0x%08X", m_header.crc32).c_str();
          break;
        default:
          break;
      }
    }
    (*m_file) << "\n";
  }
}

size_t BitGen_ANALYZER::get_next_block(std::string space,
                                       std::string block_name) {
  CFG_ASSERT(m_integrity.addr > m_current_bop_data)
  CFG_ASSERT(m_current_bop_data_index <= m_current_bop_size);
  size_t index = 0;
  if (m_current_bop_data_index == m_current_bop_size) {
    post_error(space + "  ", "Do not have enough memory to get next block");
  } else {
    CFG_ASSERT((m_current_bop_data_index % BitGen_BITSTREAM_BLOCK_SIZE) == 0);
    CFG_ASSERT((m_current_bop_size % BitGen_BITSTREAM_BLOCK_SIZE) == 0);
    CFG_ASSERT(m_integrity.size);
    CFG_ASSERT(m_integrity.remaining_size);
    CFG_ASSERT(m_integrity.remaining_size >= m_integrity.size);
    bool is_last_block = (m_current_bop_size - m_current_bop_data_index) ==
                         BitGen_BITSTREAM_BLOCK_SIZE;
    if (is_last_block || m_integrity.remaining_size >= (2 * m_integrity.size)) {
      (*m_file)
          << space.c_str()
          << CFG_print(
                 "Info: Verify %s data before using it (Use Hash at offset "
                 "0x%08lX to verify Block at offset 0x%08lX)\n",
                 block_name.c_str(),
                 size_t(m_integrity.addr) - size_t(m_current_bop_data),
                 m_current_bop_data_index)
                 .c_str();
      uint8_t sha[64];
      CFGOpenSSL::sha(m_integrity.size,
                      &m_current_bop_data[m_current_bop_data_index],
                      BitGen_BITSTREAM_BLOCK_SIZE, sha);
      if (memcmp(m_integrity.addr, sha, m_integrity.size) == 0) {
        (*m_file) << space.c_str() << "  Info: Successfully verify hash\n";
        index = m_current_bop_data_index;
        m_current_bop_data_index += BitGen_BITSTREAM_BLOCK_SIZE;
        m_integrity.addr += m_integrity.size;
        m_integrity.remaining_size -= m_integrity.size;
      } else {
        post_error(space + "  ", "Fail to verify hash");
        m_status.integrity_status = false;
      }
    } else {
      (*m_file) << space.c_str()
                << "Info: Need Hash Block to provide more hash(s)\n";
      (*m_file) << space.c_str()
                << CFG_print(
                       "Info: Use last hash entry to verify this Hash Block "
                       "(Use Hash "
                       "at offset 0x%08lX to verify Block at offset 0x%08lX)\n",
                       size_t(m_integrity.addr) - size_t(m_current_bop_data),
                       m_current_bop_data_index)
                       .c_str();
      uint8_t sha[64];
      CFGOpenSSL::sha(m_integrity.size,
                      &m_current_bop_data[m_current_bop_data_index],
                      BitGen_BITSTREAM_BLOCK_SIZE, sha);
      if (memcmp(m_integrity.addr, sha, m_integrity.size) == 0) {
        (*m_file) << space.c_str() << "  Info: Successfully verify hash\n";
        print_hash_block(space + "  ",
                         &m_current_bop_data[m_current_bop_data_index]);
        m_integrity.remaining_size = BitGen_BITSTREAM_BLOCK_SIZE;
        m_integrity.addr =
            const_cast<uint8_t*>(&m_current_bop_data[m_current_bop_data_index]);
        m_current_bop_data_index += BitGen_BITSTREAM_BLOCK_SIZE;
        // Make sure recursive won't happen
        CFG_ASSERT(m_integrity.remaining_size >= (2 * m_integrity.size));
        index = get_next_block(space, block_name);
      } else {
        post_error(space + "  ", "Fail to verify hash");
        m_status.integrity_status = false;
      }
    }
  }
  return index;
}

void BitGen_ANALYZER::print_hash_block(std::string space, const uint8_t* data) {
  (*m_file) << space.c_str() << "Block: Hash\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  (*m_file) << space.c_str() << "  Absolute | Offset   | Data     | Remark\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  size_t print_i = 0;
  size_t target_block = m_current_bop_data_index + BitGen_BITSTREAM_BLOCK_SIZE;
  for (size_t i = 0; i < BitGen_BITSTREAM_BLOCK_SIZE; i += 4) {
    uint32_t u32 = get_u32(&data[i]);
    (*m_file) << space.c_str()
              << CFG_print("  %08X | %08X | %08X |",
                           m_current_bop_offset + m_current_bop_data_index + i,
                           m_current_bop_data_index + i, u32)
                     .c_str();
    if (i == print_i && target_block < m_current_bop_size) {
      (*m_file) << CFG_print(
                       " Used to verify block at addresss of Offset 0x%08X "
                       "(Absoulte 0x%08X)",
                       target_block, m_current_bop_offset + target_block)
                       .c_str();
      print_i += m_integrity.size;
      target_block += BitGen_BITSTREAM_BLOCK_SIZE;
    }
    (*m_file) << "\n";
  }
}

void BitGen_ANALYZER::parse_payload(std::string space, const uint8_t* data,
                                    size_t size, size_t payload_addr,
                                    bool action_force_turn_off_compression,
                                    bool action_iv, uint8_t* iv,
                                    bool is_last_payload_block,
                                    std::ofstream& binfile) {
  CFG_ASSERT(size);
  CFG_ASSERT(size <= BitGen_BITSTREAM_BLOCK_SIZE);
  uint8_t plain_data[BitGen_BITSTREAM_BLOCK_SIZE] = {0};
  if (m_aes_key != nullptr) {
    (*m_file) << space.c_str() << "Info: Decrypt payload";
    if (action_iv) {
      (*m_file) << " using action IV";
    } else {
      (*m_file) << " using BOP IV";
    }
    (*m_file) << " (";
    (*m_file) << CFG_convert_bytes_to_hex_string(iv, 16).c_str();
    (*m_file) << ")";
    (*m_file) << "\n";
    CFGOpenSSL::ctr_decrypt(data, plain_data, size, m_aes_key->data(),
                            m_aes_key->size(), iv, 16, iv);
  }
  (*m_file) << space.c_str() << "Block: Payload\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  (*m_file) << space.c_str()
            << "  Absolute | Offset   | Data     | Decrpt   | Decompress\n";
  (*m_file) << space.c_str() << print_repeat_word_line("-", 100).c_str()
            << "\n";
  std::string unencrypted_data_string = print_repeat_word_line(" ", 8);
  bool proceed_dcmp = m_header.encryption == "none" || m_aes_key != nullptr;
  proceed_dcmp &=
      m_header.compression != "none" && !action_force_turn_off_compression;
  size_t remaining_size = size;
  for (size_t i = 0; i < BitGen_BITSTREAM_BLOCK_SIZE; i += 4) {
    uint32_t u32 = get_u32(&data[i]);
    uint32_t plain_u32 = 0;
    if (m_aes_key != nullptr) {
      plain_u32 = get_u32(&plain_data[i]);
      unencrypted_data_string = CFG_print("%08X", plain_u32);
    }
    (*m_file) << space.c_str()
              << CFG_print("  %08X | %08X | %08X | %s |",
                           m_current_bop_offset + payload_addr + i,
                           payload_addr + i, u32,
                           unencrypted_data_string.c_str())
                     .c_str();
    if (proceed_dcmp &&
        m_status.decompression_status !=
            BitGen_DECOMPRESS_ENGINE_ERROR_STATUS &&
        m_status.decompression_status != BitGen_DECOMPRESS_ENGINE_DONE_STATUS) {
      uint32_t decompressed_data = 0;
      if (m_aes_key != nullptr) {
        decompressed_data = plain_u32;
      } else {
        decompressed_data = u32;
      }
      m_status.decompression_status = BitGen_DECOMPRESS_ENGINE_GOOD_STATUS;
      CFG_ASSERT(m_decompressed_data_offset < 4);
      while (m_status.decompression_status ==
             BitGen_DECOMPRESS_ENGINE_GOOD_STATUS) {
        size_t dcmp_out_size = 0;
        m_status.decompression_status = m_decompress_engine.process(
            (uint8_t*)(&decompressed_data), sizeof(decompressed_data),
            &m_decompressed_data[m_decompressed_data_offset],
            sizeof(m_decompressed_data) - m_decompressed_data_offset,
            dcmp_out_size);
        if (m_status.decompression_status ==
            BitGen_DECOMPRESS_ENGINE_ERROR_STATUS) {
          m_status.status = false;
          break;
        } else if (m_status.decompression_status ==
                   BitGen_DECOMPRESS_ENGINE_NEED_INPUT_STATUS) {
          break;
        }
        CFG_ASSERT(dcmp_out_size);
        size_t total_size = m_decompressed_data_offset + dcmp_out_size;
        CFG_ASSERT(total_size <= sizeof(m_decompressed_data));
        uint8_t* dcmp_data_ptr = &m_decompressed_data[0];
        while (total_size >= 4) {
          (*m_file) << CFG_print(" %08X", get_u32(dcmp_data_ptr)).c_str();
          binfile.write((char*)(dcmp_data_ptr), 4);
          total_size -= 4;
          dcmp_data_ptr += 4;
        }
        m_decompressed_data_offset = total_size;
        if (m_decompressed_data_offset) {
          CFG_ASSERT(m_decompressed_data_offset < 4);
          if (m_decompressed_data != dcmp_data_ptr) {
            CFG_ASSERT(dcmp_data_ptr > m_decompressed_data);
            memcpy(m_decompressed_data, dcmp_data_ptr,
                   m_decompressed_data_offset);
          }
        }
      }
      if (m_status.decompression_status !=
              BitGen_DECOMPRESS_ENGINE_ERROR_STATUS &&
          m_decompressed_data_offset != 0) {
        if (m_status.decompression_status !=
            BitGen_DECOMPRESS_ENGINE_DONE_STATUS) {
          (*m_file) << CFG_print(" (Remaining: ");
        }
        (*m_file) << CFG_convert_bytes_to_hex_string(
                         m_decompressed_data, m_decompressed_data_offset, ", ")
                         .c_str();
        if (m_status.decompression_status !=
            BitGen_DECOMPRESS_ENGINE_DONE_STATUS) {
          (*m_file) << ")";
        }
      }
      if (m_status.decompression_status ==
          BitGen_DECOMPRESS_ENGINE_DONE_STATUS) {
        (*m_file) << " [DCMP: DONE]";
      }
    } else if (!proceed_dcmp) {
      if (remaining_size) {
        size_t write_size = remaining_size > 4 ? 4 : remaining_size;
        if (m_aes_key != nullptr) {
          binfile.write((char*)(&plain_u32), write_size);
        } else {
          binfile.write((char*)(&u32), write_size);
        }
        remaining_size -= write_size;
      }
    }
    (*m_file) << "\n";
  }
  memset(plain_data, 0, sizeof(plain_data));
  if (m_status.decompression_status == BitGen_DECOMPRESS_ENGINE_DONE_STATUS &&
      !is_last_payload_block) {
    post_error(
        space,
        "Decompress Engine status is DONE but this is not last payload block");
    m_status.status = false;
    m_status.decompression_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
  }
  if (is_last_payload_block && proceed_dcmp &&
      m_status.decompression_status != BitGen_DECOMPRESS_ENGINE_DONE_STATUS &&
      m_status.decompression_status != BitGen_DECOMPRESS_ENGINE_ERROR_STATUS) {
    post_error(space,
               "This is already last block of payload but Decompress Engine is "
               "still not DONE");
    m_status.status = false;
    m_status.decompression_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
  }
  if (is_last_payload_block && proceed_dcmp &&
      m_status.decompression_status == BitGen_DECOMPRESS_ENGINE_DONE_STATUS &&
      m_decompressed_data_offset != 0) {
    post_error(space, CFG_print("This is the end of decompression, but there "
                                "is remaining %d Byte(s) left",
                                m_decompressed_data_offset));
    m_status.status = false;
    m_status.decompression_status = BitGen_DECOMPRESS_ENGINE_ERROR_STATUS;
  }
}

void BitGen_ANALYZER::post_warning(const std::string& space,
                                   const std::string& msg, bool new_line) {
  CFG_ASSERT(m_file != nullptr);
  (*m_file) << space << "Warning: " << msg.c_str();
  if (new_line) {
    (*m_file) << "\n";
  }
  CFG_POST_WARNING(msg.c_str());
}

void BitGen_ANALYZER::post_error(const std::string& space,
                                 const std::string& msg, bool new_line) {
  CFG_ASSERT(m_file != nullptr);
  (*m_file) << space << "Error: " << msg.c_str();
  if (new_line) {
    (*m_file) << "\n";
  }
  CFG_POST_ERR(msg.c_str());
}

void BitGen_ANALYZER::push_error(std::vector<std::string>& msgs,
                                 const std::string& msg,
                                 const std::string space,
                                 const std::string new_line) {
  CFG_ASSERT(m_file != nullptr);
  msgs.push_back(
      CFG_print("%sError: %s%s", space.c_str(), msg.c_str(), new_line.c_str()));
  CFG_POST_ERR(msg.c_str());
}

void BitGen_ANALYZER::update_bitstream_end_size(std::vector<uint8_t>& data,
                                                std::string& error_msg) {
  std::vector<size_t> sizes = parse(data, false, true, error_msg, false);
  if (error_msg.empty()) {
    CFG_ASSERT(sizes.size());
    size_t start_index = 0;
    size_t total_size = data.size();
    for (size_t i = 0, j = sizes.size() - 1; i < sizes.size(); i++, j--) {
      BitGen_PACKER::update_bitstream_ending_size(&data[start_index],
                                                  total_size, j == 0);
      start_index += sizes[i];
      total_size -= sizes[i];
    }
    parse(data, true, true, error_msg, false);
    CFG_ASSERT_MSG(error_msg.empty(), error_msg.c_str());
  }
}

// Basic parser
//  - only check individual BOP
//  - check identifier, size and CRC
//  - check end size if boolean is set
std::vector<size_t> BitGen_ANALYZER::parse(const std::vector<uint8_t>& data,
                                           bool check_end_size, bool check_crc,
                                           std::string& error_msg,
                                           bool print_msg) {
  std::string msg = "\nBitstream Anayzer\n";
  std::vector<size_t> sizes;
  if (data.size() == 0 || ((data.size() % BitGen_BITSTREAM_BLOCK_SIZE) != 0)) {
    error_msg = CFG_print("Bitstream has invalid Bytes size %ld", data.size());
  } else {
    error_msg = "";
    size_t index = 0;
    while (index < data.size()) {
      std::string identifier = get_null_terminate_string(&data[index], 4);
      int identifier_index = CFG_find_string_in_vector(
          BitGen_BITSTREAM_SUPPORTED_BOP_IDENTIFIER, identifier);
      if (identifier_index == -1) {
        error_msg =
            CFG_print("BOP Identifier %s is not supported", identifier.c_str());
        sizes.clear();
        break;
      }
      size_t size = (size_t)(get_u64(&data[index + 8]));
      if (size == 0 || ((size % BitGen_BITSTREAM_BLOCK_SIZE) != 0) ||
          (index + size) > data.size()) {
        error_msg = CFG_print("BOP Identifier %s has invalid Bytes size %ld",
                              identifier.c_str(), size);
        sizes.clear();
        break;
      }
      if (check_crc) {
        uint32_t crc32 = CFG_crc32(&data[index], 0x7FC);
        uint32_t bitstream_crc = get_u32(&data[index + 0x7FC]);
        if (crc32 != bitstream_crc) {
          error_msg = CFG_print(
              "BOP Identifier %s has invalid CRC - expect 0x%08X but found "
              "0x%08X",
              identifier.c_str(), crc32, bitstream_crc);
          sizes.clear();
          break;
        }
      }
      if (check_end_size) {
        bool is_last_block = (data[index + 0x780] & 1) != 0;
        size_t end_size = (size_t)(get_u64(&data[index + 0x788]));
        if ((index + end_size) != data.size()) {
          error_msg = CFG_print(
              "BOP Identifier %s has invalid End Bytes size - Expect %ld but "
              "found",
              identifier.c_str(), data.size() - index, end_size);
          sizes.clear();
          break;
        }
        if (is_last_block) {
          if ((index + size) != data.size()) {
            error_msg = CFG_print(
                "Last BOP block bit had been set, but BOP size does not "
                "indicate this is last block");
            sizes.clear();
            break;
          }
        } else {
          if ((index + size) == data.size()) {
            error_msg = CFG_print(
                "Last BOP block bit had been not set, but BOP size indicates "
                "this is last block");
            sizes.clear();
            break;
          }
        }
      }
      index += size;
      sizes.push_back(size);
      msg = CFG_print("%s  Found Identifier %s\n    Size: 0x%016X (%ld)\n",
                      msg.c_str(), identifier.c_str(), size, size);
    }
  }
  if (error_msg.empty() && print_msg) {
    CFG_post_msg(msg, "");
  }
  return sizes;
}

void BitGen_ANALYZER::parse_debug(const std::string& input_filepath,
                                  const std::string& output_filepath,
                                  std::vector<uint8_t>& aes_key) {
  std::vector<uint8_t> data;
  CFG_read_binary_file(input_filepath, data);
  std::string error_msg = "";
  std::vector<size_t> sizes = parse(data, true, false, error_msg, false);
  if (error_msg.empty()) {
    std::ofstream file;
    file.open(output_filepath.c_str());
    CFG_ASSERT(file.is_open());
    CFG_ASSERT(file.good());
    file << "File input: " << input_filepath.c_str() << "\n";
    file << "  BOP count: " << sizes.size() << "\n";
    BitGen_ANALYZER analyzer(output_filepath, &file,
                             aes_key.size() == 0 ? nullptr : &aes_key);
    size_t start_index = 0;
    uint32_t bop_index = 0;
    for (auto& size : sizes) {
      bool status =
          analyzer.parse_bop(&data[start_index], size, bop_index, start_index);
      start_index += size;
      bop_index++;
      if (!status) {
        CFG_POST_ERR(
            "Bitstream %s is corrupted - see debug text file for more detail",
            input_filepath.c_str());
        break;
      }
    }
    file.close();
  } else {
    CFG_POST_ERR("Bitstream %s is corrupted - %s", input_filepath.c_str(),
                 error_msg.c_str());
  }
}
