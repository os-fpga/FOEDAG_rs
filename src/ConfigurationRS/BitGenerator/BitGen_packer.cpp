#include "BitGen_packer.h"

#include <map>

#include "CFGCrypto/CFGOpenSSL.h"

BitGen_BITSTREAM_BOP_FIELD::~BitGen_BITSTREAM_BOP_FIELD() {
  memset(iv, 0, sizeof(iv));
}

BitGen_BITSTREAM_ACTION::BitGen_BITSTREAM_ACTION(uint16_t a) { action = a; }

BitGen_BITSTREAM_ACTION::BitGen_BITSTREAM_ACTION(const std::string& a) {
  CFG_ASSERT(a.size());
}

BitGen_BITSTREAM_ACTION::~BitGen_BITSTREAM_ACTION() {
  if (field.size()) {
    memset(&field[0], 0, field.size());
    field.clear();
  }
  if (iv.size()) {
    memset(&iv[0], 0, iv.size());
    iv.clear();
  }
  if (payload.size()) {
    memset(&payload[0], 0, payload.size());
    payload.clear();
  }
}

BitGen_BITSTREAM_BOP::~BitGen_BITSTREAM_BOP() {
  while (actions.size()) {
    CFG_MEM_DELETE(actions.back());
    actions.pop_back();
  }
}

BitGen_BITSTREAM_BLOCK::BitGen_BITSTREAM_BLOCK(BitGen_BITSTREAM_BLOCK_TYPE t)
    : type(t) {
  data.resize(BitGen_BITSTREAM_BLOCK_SIZE);
  memset(&data[0], 0, data.size());
}

BitGen_BITSTREAM_BLOCK::~BitGen_BITSTREAM_BLOCK() {
  if (data.size()) {
    memset(&data[0], 0, data.size());
    data.clear();
  }
}

const std::map<const std::string, const uint8_t> BitGen_PACKER_U8_ENUM_MAP = {
    // checksum
    {"flecther32", 0x10},
    // Compression
    {"dcmp0", 0x10},
    {"dcmp1", 0x11},
    // Integrity
    {"sha256", 0x10},
    {"sha384", 0x11},
    {"sha512", 0x12},
    // Encryption
    {"ctr128", 0x10},
    {"ctr196", 0x11},
    {"ctr256", 0x12},
    {"gcm128", 0x20},
    {"gcm196", 0x21},
    {"gcm256", 0x22},
    // Authentication
    {"ecdsa256", 0x10},
    {"ecdsa384", 0x11},
    {"rsa2048", 0x20},
    {"brainpool256", 0x30},
    {"brainpool384", 0x31},
    {"sm2", 0x40}};

static void BitGen_PACKER_set_u8_enum(uint8_t* data,
                                      const std::string& enum_string) {
  auto iter = BitGen_PACKER_U8_ENUM_MAP.find(enum_string);
  CFG_ASSERT(iter != BitGen_PACKER_U8_ENUM_MAP.end());
  (*data) = iter->second;
}

#define BitGen_PACKER_get_u8_enum(str)     \
  ({                                       \
    uint8_t temp = 0;                      \
    BitGen_PACKER_set_u8_enum(&temp, str); \
    temp;                                  \
  })

static void BitGen_PACKER_gen_bop_header_basic_field(
    BitGen_BITSTREAM_BOP_FIELD& field, BitGen_BITSTREAM_BLOCK*& header,
    bool compress) {
  // Identifier - byte [3:0]
  CFG_ASSERT(field.identifier.size());
  CFG_ASSERT(field.identifier.size() <= 4);
  memcpy(&header->data[0], field.identifier.c_str(), field.identifier.size());
  // Version - byte [0x7:0x4]
  memcpy(&header->data[0x4], (void*)(&field.version), sizeof(field.version));
  // Size - byte [0xF:0x8]
  // Tool - byte [0x2F:0x10]
  CFG_ASSERT(field.tool.size() <= 32);
  memcpy(&header->data[0x10], field.tool.c_str(), field.tool.size());
  // OPN - byte [0x3F:0x30]
  CFG_ASSERT(field.opn.size() <= 16);
  memcpy(&header->data[0x30], field.opn.c_str(), field.opn.size());
  // JTAG ID - byte [0x43:0x40]
  memcpy(&header->data[0x40], (void*)(&field.jtag_id), sizeof(field.jtag_id));
  // JTAG Mask - byte [0x47:0x44]
  memcpy(&header->data[0x40], (void*)(&field.jtag_mask),
         sizeof(field.jtag_mask));
  // Reserved - byte [0x4F:0x48]
  // Obscured Chipid - byte [0x50]
  memcpy(&header->data[0x50], (void*)(&field.chipid), sizeof(field.chipid));
  // Obscured Reserved - byte [0x5B:0x51]
  // Obscured CRC - byte [0x5F:0x5C]
  uint32_t crc32 = CFG_crc32(&header->data[0x50], 12);
  memcpy(&header->data[0x5C], (void*)(&crc32), sizeof(crc32));
  // Checksum - byte [0x60]
  memcpy(&header->data[0x60], (void*)(&field.checksum), sizeof(field.checksum));
  // Compression - byte [0x61] - currently only support DCMP0
  uint8_t compression = compress ? 0x10 : 0;
  memcpy(&header->data[0x61], (void*)(&compression), sizeof(compression));
  // Integrity - byte [0x62] - impossible there is no interity
  CFG_ASSERT(field.integrity != 0);
  memcpy(&header->data[0x62], (void*)(&field.integrity),
         sizeof(field.integrity));
}

static void BitGen_PACKER_gen_bop_header_encryption_field(
    BitGen_BITSTREAM_BOP_FIELD& field, BitGen_BITSTREAM_BLOCK*& header,
    std::vector<uint8_t>& aes_key) {
  CFG_ASSERT(aes_key.size() == 0 || aes_key.size() == 16 ||
             aes_key.size() == 32);
  if (aes_key.size()) {
    // Encryption - byte [0x80]
    header->data[0x80] = aes_key.size() == 16 ? 0x10 : 0x12;
    // Challenge
    std::vector<uint8_t> challenge(64);
    std::vector<uint8_t> encrypted_challenge(challenge.size());
    uint32_t crc = 0;
    CFGOpenSSL::generate_random_data(&challenge[0],
                                     challenge.size() - sizeof(uint32_t));
    crc = CFG_crc32(&challenge[0], challenge.size() - sizeof(uint32_t));
    memcpy(&challenge[challenge.size() - sizeof(uint32_t)], (void*)(&crc),
           sizeof(crc));
    // Encrypt
    CFGOpenSSL::ctr_encrypt(&challenge[0], &encrypted_challenge[0],
                            challenge.size(), &aes_key[0], aes_key.size(),
                            field.iv, sizeof(field.iv));
    // Challenge - random data - byte [0x27F:0x240]
    memcpy(&header->data[0x240], &encrypted_challenge[0],
           encrypted_challenge.size());
    // IV - byte [0x28F:0x280]
    memcpy(&header->data[0x280], field.iv, sizeof(field.iv));
    // Increment IV
    uint32_t iv = 0;
    memcpy((void*)(&iv), field.iv, sizeof(iv));
    iv++;
    memcpy(field.iv, (void*)(&iv), sizeof(iv));
  }
}

static void BitGen_PACKER_update_action(
    std::vector<BitGen_BITSTREAM_BLOCK*>& blocks, uint8_t* data, size_t size,
    uint8_t*& action_data, size_t& action_remaining_size) {
  if (action_remaining_size >= size) {
    // We have enough space
    memcpy(action_data, data, size);
    action_data += size;
    action_remaining_size -= size;
  } else {
    // We do not have enough space, create new one
    BitGen_BITSTREAM_BLOCK* action =
        CFG_MEM_NEW(BitGen_BITSTREAM_BLOCK, BitGen_BITSTREAM_ACTION_BLOCK);
    // To prevent unlimited recursive
    CFG_ASSERT(size <= action->data.size());
    // Action data has new pointer now
    action_data = &action->data[0];
    // New remaining value as well
    action_remaining_size = action->data.size();
    blocks.push_back(action);
    BitGen_PACKER_update_action(blocks, data, size, action_data,
                                action_remaining_size);
  }
}

static void BitGen_PACKER_update_hash(
    std::vector<BitGen_BITSTREAM_BLOCK*>& blocks, BitGen_BITSTREAM_BLOCK* block,
    size_t hash_size, uint8_t*& hash_data, size_t& hash_remaining_size,
    bool is_last_block) {
  CFG_ASSERT(block != nullptr);
  CFG_ASSERT(block->data.size());
  CFG_ASSERT(hash_remaining_size >= hash_size);
  if (hash_remaining_size >= (2 * hash_size) || is_last_block) {
    // We have enough space
    CFGOpenSSL::sha((uint8_t)(hash_size), &block->data[0], block->data.size(),
                    hash_data);
    hash_data += hash_size;
    hash_remaining_size -= hash_size;
    blocks.push_back(block);
  } else {
    // We do not have enough space, create new one
    // But at this point the hash block is still empty, hence we store the
    // pointer of last hash addr, and only update hash block's hash pointer at
    // the very end
    BitGen_BITSTREAM_BLOCK* hash =
        CFG_MEM_NEW(BitGen_BITSTREAM_BLOCK, BitGen_BITSTREAM_HASH_BLOCK);
    hash->hash_block_hash_data_ptr = hash_data;
    // To prevent unlimited recursive
    CFG_ASSERT(hash->data.size() >= (2 * hash_size));
    // Hash data has new pointer now
    hash_data = &hash->data[0];
    // New remaining value as well
    hash_remaining_size = hash->data.size();
    blocks.push_back(hash);
    BitGen_PACKER_update_hash(blocks, block, hash_size, hash_data,
                              hash_remaining_size, is_last_block);
  }
}

static void BitGen_PACKER_gen_action(
    BitGen_BITSTREAM_BOP*& bop, BitGen_BITSTREAM_ACTION*& action,
    std::vector<BitGen_BITSTREAM_BLOCK*>& blocks, uint8_t*& action_data,
    size_t& action_remaining_size, uint8_t checksum, bool compress,
    std::vector<uint8_t>& aes_key) {
  CFG_ASSERT(action != nullptr);
  CFG_ASSERT(action->action);
  CFG_ASSERT((action->action & 0xF000) == 0);
  bool cmd_is_forced_to_turn_off_compress = false;
  bool cmd_is_forced_to_use_dedicated_iv = false;
  std::vector<uint8_t> action_header;
  std::vector<uint8_t> payload;
  // Prepare payload so we will know the size
  if (action->payload.size()) {
    if (compress) {
      CFG_compress(&action->payload[0], action->payload.size(), payload);
      if (payload.size() >= action->payload.size()) {
        memset(&payload[0], 0, payload.size());
        payload.clear();
        cmd_is_forced_to_turn_off_compress = true;
      }
    }
    if (payload.empty()) {
      payload.insert(payload.end(), action->payload.begin(),
                     action->payload.end());
    }
  }
  // Encrypt
  if (payload.size() > 0 && aes_key.size() > 0) {
    CFG_ASSERT(aes_key.size() == 16 || aes_key.size() == 32);
    std::vector<uint8_t> encrypted_payload(payload.size());
    if (action->iv.size()) {
      cmd_is_forced_to_use_dedicated_iv = true;
      CFG_ASSERT(action->iv.size() == 16);
      CFGOpenSSL::ctr_encrypt(&payload[0], &encrypted_payload[0],
                              payload.size(), &aes_key[0], aes_key.size(),
                              &action->iv[0], action->iv.size());
    } else {
      CFGOpenSSL::ctr_encrypt(&payload[0], &encrypted_payload[0],
                              payload.size(), &aes_key[0], aes_key.size(),
                              bop->field.iv, sizeof(bop->field.iv));
    }
    // Copy to payload
    memcpy(&payload[0], &encrypted_payload[0], payload.size());
    memset(&encrypted_payload[0], 0, encrypted_payload.size());
    // Increment IV
    if (!cmd_is_forced_to_use_dedicated_iv) {
      uint32_t iv = 0;
      memcpy((void*)(&iv), bop->field.iv, sizeof(iv));
      iv++;
      memcpy(bop->field.iv, (void*)(&iv), sizeof(iv));
    }
  }
  // CMD
  CFG_append_u16(action_header, action->action);
  // Size (unknown yet)
  CFG_append_u16(action_header, 0);
  // Payload size
  if (action->payload.size()) {
    CFG_ASSERT(payload.size());
    CFG_append_u32(action_header, (uint32_t)(payload.size()));
    // Checksum if applicable
    if (action->has_checksum) {
      uint32_t checksum_size = 0;
      uint64_t checksum_value = BitGen_PACKER::calc_checksum(
          action->payload, checksum, checksum_size);
      CFG_ASSERT(checksum_size == 4 || checksum_size == 8);
      for (uint32_t i = 0; i < checksum_size; i++) {
        CFG_append_u8(action_header, (uint8_t)(checksum_value));
        checksum_value >>= 8;
      }
    }
  } else {
    CFG_ASSERT(payload.empty());
    CFG_append_u32(action_header, 0);
  }
  // Other field
  if (action->field.size()) {
    CFG_ASSERT((action->field.size() % 4) == 0);
    action_header.insert(action_header.end(), action->field.begin(),
                         action->field.end());
  }
  if (cmd_is_forced_to_use_dedicated_iv) {
    CFG_ASSERT(action->iv.size() == 16);
    action_header.insert(action_header.end(), action->iv.begin(),
                         action->iv.end());
  }
  CFG_ASSERT(action_header.size());
  CFG_ASSERT((action_header.size() % 4) == 0);
  // not legal if you cannot fit action in one 2k block
  CFG_ASSERT(action_header.size() <= BitGen_BITSTREAM_BLOCK_SIZE);
  // Update command and size last
  if (action->payload.size() != 0 && action->has_checksum) {
    action_header[1] |= 0x10;
  }
  if (cmd_is_forced_to_turn_off_compress) {
    action_header[1] |= 0x20;
  }
  if (cmd_is_forced_to_use_dedicated_iv) {
    action_header[1] |= 0x40;
  }
  uint16_t action_size = action_header.size();
  memcpy(&action_header[2], (void*)(&action_size), sizeof(action_size));
  BitGen_PACKER_update_action(blocks, &action_header[0], action_header.size(),
                              action_data, action_remaining_size);
  memset(&action_header[0], 0, action_header.size());
  action_header.clear();
  // Create payload
  while (payload.size()) {
    BitGen_BITSTREAM_BLOCK* data =
        CFG_MEM_NEW(BitGen_BITSTREAM_BLOCK, BitGen_BITSTREAM_DATA_BLOCK);
    size_t data_size = payload.size();
    if (data_size > data->data.size()) {
      data_size = data->data.size();
    }
    memcpy(&data->data[0], &payload[0], data_size);
    memset(&payload[0], 0, data_size);
    payload.erase(payload.begin(), payload.begin() + data_size);
    blocks.push_back(data);
  }
}

static void BitGen_PACKER_gen_actions(
    BitGen_BITSTREAM_BOP*& bop, std::vector<BitGen_BITSTREAM_BLOCK*>& blocks,
    uint8_t* action_data, size_t action_remaining_size, uint8_t checksum,
    bool compress, std::vector<uint8_t>& aes_key) {
  CFG_ASSERT(bop->actions.size());
  // Version
  const uint32_t ACTION_VERSION = 0;
  BitGen_PACKER_update_action(blocks, (uint8_t*)(&ACTION_VERSION),
                              sizeof(ACTION_VERSION), action_data,
                              action_remaining_size);
  // Total action
  uint32_t action_count = (uint32_t)(bop->actions.size());
  BitGen_PACKER_update_action(blocks, (uint8_t*)(&action_count),
                              sizeof(action_count), action_data,
                              action_remaining_size);
  // Loop through the action
  for (auto& action : bop->actions) {
    BitGen_PACKER_gen_action(bop, action, blocks, action_data,
                             action_remaining_size, checksum, compress,
                             aes_key);
  }
}

static void BitGen_PACKER_update_hash(
    std::vector<BitGen_BITSTREAM_BLOCK*>& blocks) {
  CFG_ASSERT(blocks.size());
  // first block must be header
  CFG_ASSERT(blocks.front()->type == BitGen_BITSTREAM_HEADER_BLOCK);
  std::vector<BitGen_BITSTREAM_BLOCK*> temp;
  for (auto& block : blocks) {
    temp.push_back(block);
  }
  blocks.clear();
  BitGen_BITSTREAM_BLOCK* header = temp.front();
  temp.erase(temp.begin());
  blocks.push_back(header);
  CFG_ASSERT(header->data[0x62] == 0x10 || header->data[0x62] == 0x11 ||
             header->data[0x62] == 0x12);
  size_t hash_size =
      header->data[0x62] == 0x10 ? 32 : (header->data[0x62] == 0x11 ? 48 : 64);
  uint8_t* hash_data = &header->data[0x200];
  size_t hash_remaining_size = hash_size;
  if (temp.size()) {
    while (temp.size()) {
      BitGen_BITSTREAM_BLOCK* block = temp.front();
      temp.erase(temp.begin());
      CFG_ASSERT(block->type == BitGen_BITSTREAM_ACTION_BLOCK ||
                 block->type == BitGen_BITSTREAM_DATA_BLOCK);
      BitGen_PACKER_update_hash(blocks, block, hash_size, hash_data,
                                hash_remaining_size, temp.empty());
    }
    for (auto iter = blocks.rbegin(); iter != blocks.rend(); iter++) {
      BitGen_BITSTREAM_BLOCK* block = *iter;
      if (block->type == BitGen_BITSTREAM_HASH_BLOCK) {
        CFG_ASSERT(block->hash_block_hash_data_ptr != nullptr);
        CFGOpenSSL::sha((uint8_t)(hash_size), &block->data[0],
                        block->data.size(), block->hash_block_hash_data_ptr);
        block->hash_block_hash_data_ptr = nullptr;
      }
    }
  } else {
    // special case, there is no block other than header
    CFGOpenSSL::sha((uint8_t)(hash_size), &header->data[0xC0], 0x140,
                    &header->data[0x200]);
  }
}

static void BitGen_PACKER_update_bitstream_size(
    std::vector<BitGen_BITSTREAM_BLOCK*>& blocks) {
  CFG_ASSERT(blocks.size());
  // first block must be header
  BitGen_BITSTREAM_BLOCK* header = blocks.front();
  CFG_ASSERT(header->type == BitGen_BITSTREAM_HEADER_BLOCK);
  size_t block_size = header->data.size();
  CFG_ASSERT(block_size);
  for (auto& block : blocks) {
    CFG_ASSERT(block_size == block->data.size());
  }
  uint64_t size = (uint64_t)(blocks.size() * block_size);
  memcpy(&header->data[0x8], (void*)(&size), sizeof(size));
}

static void BitGen_PACKER_sign(BitGen_BITSTREAM_BLOCK*& header,
                               CFGCrypto_KEY*& key) {
  CFG_ASSERT(header->type == BitGen_BITSTREAM_HEADER_BLOCK);
  if (key != nullptr) {
    // Authentication - byte [0x81]
    header->data[0x81] = key->get_bitstream_signing_algo();
    std::vector<uint8_t> public_key;
    key->get_public_key(public_key, 4);
    CFG_ASSERT(public_key.size());
    CFG_ASSERT(public_key.size() <= 272);
    memcpy(&header->data[0x570], &public_key[0], public_key.size());
    size_t signature_size = CFGOpenSSL::sign_message(
        &header->data[0], 0x680, &header->data[0x680], 0x100, key);
    CFG_ASSERT(signature_size <= 0x100);
    memset(&public_key[0], 0, public_key.size());
    public_key.clear();
  }
}

static void BitGen_PACKER_finalize(BitGen_BITSTREAM_BLOCK*& header) {
  CFG_ASSERT(header->type == BitGen_BITSTREAM_HEADER_BLOCK);
  uint32_t crc32 = 0;
  crc32 = CFG_crc32(&header->data[0], header->data.size() - sizeof(crc32));
  memcpy(&header->data[header->data.size() - sizeof(crc32)], (void*)(&crc32),
         sizeof(crc32));
}

static void BitGen_PACKER_gen_bop_bitstream(
    BitGen_BITSTREAM_BOP*& bop, std::vector<BitGen_BITSTREAM_BLOCK*>& blocks,
    bool compress, std::vector<uint8_t>& aes_key, CFGCrypto_KEY*& key) {
  CFG_ASSERT(bop->actions.size());
  BitGen_BITSTREAM_BLOCK* header =
      CFG_MEM_NEW(BitGen_BITSTREAM_BLOCK, BitGen_BITSTREAM_HEADER_BLOCK);
  blocks.push_back(header);
  BitGen_PACKER_gen_bop_header_basic_field(bop->field, header, compress);
  BitGen_PACKER_gen_bop_header_encryption_field(bop->field, header, aes_key);
  BitGen_PACKER_gen_actions(bop, blocks, &header->data[0xC0], 0x140,
                            header->data[0x60], compress, aes_key);
  BitGen_PACKER_update_hash(blocks);
  BitGen_PACKER::obscure(&header->data[0x50], &header->data[0x200]);
  BitGen_PACKER_update_bitstream_size(blocks);
  BitGen_PACKER_sign(header, key);
  BitGen_PACKER_finalize(header);
}

void BitGen_PACKER::update_bitstream_ending_size(uint8_t* const data,
                                                 uint64_t ending_size,
                                                 bool is_last_bop) {
  uint32_t crc32 = CFG_crc32(data, 0x7FC);
  CFG_ASSERT(memcmp(&data[0x7FC], (void*)(&crc32), sizeof(crc32)) == 0);
  if (is_last_bop) {
    data[0x780] |= 0x01;
  } else {
    data[0x780] &= 0xFE;
  }
  memcpy(&data[0x788], (void*)(&ending_size), sizeof(ending_size));
  crc32 = CFG_crc32(data, 0x7FC);
  memcpy(&data[0x7FC], (void*)(&crc32), sizeof(crc32));
}

void BitGen_PACKER::generate_bitstream(std::vector<BitGen_BITSTREAM_BOP*>& bops,
                                       std::vector<uint8_t>& data,
                                       bool compress,
                                       std::vector<uint8_t>& aes_key,
                                       CFGCrypto_KEY*& key) {
  CFG_ASSERT(bops.size());
  // Track each BOP size
  size_t start_index = data.size();
  std::vector<size_t> tracking_size;
  for (BitGen_BITSTREAM_BOP*& bop : bops) {
    size_t temp_start_index = data.size();
    std::vector<BitGen_BITSTREAM_BLOCK*> bop_blocks;
    BitGen_PACKER_gen_bop_bitstream(bop, bop_blocks, compress, aes_key, key);
    CFG_ASSERT(bop_blocks.size());
    for (BitGen_BITSTREAM_BLOCK*& block : bop_blocks) {
      CFG_ASSERT(block != nullptr);
      data.insert(data.end(), block->data.begin(), block->data.end());
    }
    while (bop_blocks.size()) {
      CFG_MEM_DELETE(bop_blocks.back());
      bop_blocks.pop_back();
    }
    tracking_size.push_back(data.size() - temp_start_index);
  }
  size_t total_size = data.size() - start_index;
  for (size_t i = 0, j = tracking_size.size() - 1; i < tracking_size.size();
       i++, j--) {
    update_bitstream_ending_size(&data[start_index], total_size, j == 0);
    start_index += tracking_size[i];
    total_size -= tracking_size[i];
  }
}

std::string BitGen_PACKER::get_feature_enum_string(
    uint8_t feature_enum, std::vector<std::string> features, bool& status,
    bool allow_none) {
  CFG_ASSERT(features.size());
  if (allow_none && feature_enum == 0) {
    return "none";
  } else {
    std::string feature_string = "unknown";
    for (auto feature : features) {
      CFG_ASSERT(feature != "unknown");
      if (feature_enum == BitGen_PACKER_get_u8_enum(feature)) {
        feature_string = feature;
        break;
      }
    }
    if (feature_string == "unknown") {
      status = false;
    }
    return feature_string;
  }
}

void BitGen_PACKER::obscure(uint8_t* obscure_addr, const uint8_t* hash_addr) {
  for (uint32_t obscure = 0; obscure < 4; obscure++) {
    for (size_t i = 0, j = 15; i < 16; i++, j--) {
      if (obscure == 0 || obscure == 2) {
        obscure_addr[i] ^= hash_addr[i];
      } else {
        obscure_addr[i] ^= hash_addr[j];
      }
    }
    hash_addr += 16;
  }
}

uint64_t BitGen_PACKER::calc_checksum(std::vector<uint8_t>& data, uint8_t type,
                                      uint32_t& checksum_size) {
  uint64_t checksum = 0;
  if (type == 0x10) {
    // Flecther-32
    CFG_ASSERT(data.size() > 0 && (data.size() % 4) == 0);
    uint16_t* data_ptr = reinterpret_cast<uint16_t*>(&data[0]);
    size_t u16_length = data.size() / 2;
    uint32_t c0 = 0;
    uint32_t c1 = 0;
    uint32_t type_checksum = 0;
    for (size_t i = 0; i < u16_length;) {
      c0 += data_ptr[i++];
      c1 += c0;
    }
    type_checksum = (c1 << 16) | ((-(c1 + c0)) & 0x0000ffff);
    checksum = (uint64_t)(type_checksum);
    checksum_size = (uint64_t)(sizeof(type_checksum));
  } else {
    CFG_INTERNAL_ERROR("Does not support checksum type %d", type);
  }
  return checksum;
}
