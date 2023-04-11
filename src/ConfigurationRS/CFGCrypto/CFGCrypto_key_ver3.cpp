#include "openssl/core_names.h"

CFGCrypto_KEY::CFGCrypto_KEY() {
  CFGOpenSSL::init_openssl();
  memset(m_public_key, 0, sizeof(m_public_key));
}

CFGCrypto_KEY::CFGCrypto_KEY(const std::string& filepath,
                             const std::string& passphrase,
                             bool expected_private) {
  CFGOpenSSL::init_openssl();
  memset(m_public_key, 0, sizeof(m_public_key));
  initial(filepath, passphrase, expected_private);
}

CFGCrypto_KEY::CFGCrypto_KEY(const std::string& key_type,
                             const uint8_t* pub_key,
                             const uint32_t pub_key_size)
    : m_filepath(""), m_is_private(false) {
  CFGOpenSSL::init_openssl();
  memset(m_public_key, 0, sizeof(m_public_key));
  m_key_info = CFGOpenSSL::get_key_info(key_type);
  CFG_ASSERT(m_key_info != nullptr);
  m_is_ec = m_key_info->nid != NID_rsa;
  if (m_is_ec) {
    EVP_PKEY* key = nullptr;
    CFG_ASSERT(pub_key_size == (2 * m_key_info->size));
    CFG_ASSERT((pub_key_size + 1) <= sizeof(m_public_key));
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(
        OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>(key_type.c_str()), 0);
    params[1] = OSSL_PARAM_construct_end();
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    CFG_ASSERT(ctx != nullptr);
    CFG_ASSERT(EVP_PKEY_fromdata_init(ctx) == 1);
    CFG_ASSERT(EVP_PKEY_fromdata(ctx, &key,
                                 OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS,
                                 params) == 1);
    CFG_ASSERT(key != nullptr);
    m_public_key[0] = 0x4;
    memcpy(&m_public_key[1], pub_key, pub_key_size);
    uint8_t* ptr = &m_public_key[0];
    key = d2i_PublicKey(EVP_PKEY_EC, &key, (const uint8_t**)(&ptr),
                        long(pub_key_size + 1));
    m_evp_key = key;
    CFG_ASSERT(m_evp_key != nullptr);
    EVP_PKEY_CTX_free(ctx);
  } else {
    CFG_ASSERT((m_key_info->size + 4) <= pub_key_size);
    CFG_ASSERT((size_t)(m_key_info->size + 4) <= sizeof(m_public_key));
    // Always construct a non-optimized DER
    // 0x30 ? ? ?
    //   0x02 ? ? ? pubkey        | +1
    //   0x02 ? set_word_size (6) | +1
    // Make sure pubkey size is 2 bytes
    CFG_ASSERT((m_key_info->size + 1) > 127 && (m_key_info->size + 1) <= 65535);
    uint32_t total_len = (4 + m_key_info->size + 1) + (2 + 4 + 1);
    CFG_ASSERT(total_len > 127 && total_len <= 65535);
    CFG_ASSERT(size_t(total_len) <= sizeof(m_public_key));
    size_t index = 0;
    m_public_key[index++] = 0x30;
    m_public_key[index++] = 0x82;
    m_public_key[index++] = (total_len >> 8) & 0xFF;
    m_public_key[index++] = (total_len)&0xFF;
    // Key integer
    m_public_key[index++] = 0x02;
    m_public_key[index++] = 0x82;
    m_public_key[index++] = ((m_key_info->size + 1) >> 8) & 0xFF;
    m_public_key[index++] = (m_key_info->size + 1) & 0xFF;
    // hardcode to 0 no matter the key is signed/unsigned
    m_public_key[index++] = 0;
    memcpy(&m_public_key[index], pub_key, m_key_info->size);
    index += m_key_info->size;
    // set_word_size integer
    m_public_key[index++] = 0x02;
    m_public_key[index++] = 0x5;
    m_public_key[index++] = 0;
    uint32_t set_word_size = 0;
    memcpy((uint8_t*)(&set_word_size), &pub_key[m_key_info->size],
           sizeof(set_word_size));
    for (size_t i = 0, j = sizeof(set_word_size) - 1; i < sizeof(set_word_size);
         i++, j--) {
      m_public_key[index++] = (set_word_size >> (j * 8)) & 0xFF;
    }
    CFG_ASSERT(index == (total_len + 4));
#if ENABLE_DEBUG
    printf("Constructed DER: %ld\n ", index);
    for (uint32_t i = 0; i < uint32_t(index); i++) {
      printf(" %02X", m_public_key[i]);
      if ((i % 16) == 15) {
        printf("\n ");
      }
    }
    printf("\n");
#endif
    uint8_t* ptr = &m_public_key[0];
    m_evp_key = d2i_PublicKey(EVP_PKEY_RSA, nullptr, (const uint8_t**)(&ptr),
                              long(index));
    CFG_ASSERT(m_evp_key != nullptr);
  }
  // Double EC_KEY_check_key
  get_public_key();
  CFG_ASSERT(m_public_key_size == pub_key_size);
  CFG_ASSERT(memcmp(m_public_key, pub_key, pub_key_size) == 0);
}

CFGCrypto_KEY::~CFGCrypto_KEY() {
  if (m_evp_key != nullptr) {
    EVP_PKEY_free(get_evp_pkey(m_evp_key));
    m_evp_key = nullptr;
  }
  memset(m_public_key, 0, sizeof(m_public_key));
}

void CFGCrypto_KEY::initial(const std::string& filepath,
                            const std::string& passphrase,
                            bool expected_private) {
  m_filepath = filepath;
  m_is_private = false;
  if (expected_private) {
    // If the expected key is private, then it must be private
    m_evp_key = CFGOpenSSL::read_private_key(filepath, passphrase, m_is_ec);
    CFG_ASSERT(m_evp_key != nullptr);
    m_is_private = true;
  } else {
    // If the expected key is public, it can be both private and public, because
    // public can be retrieve from private
    m_evp_key =
        CFGOpenSSL::read_public_key(filepath, passphrase, m_is_ec, true);
    if (m_evp_key == nullptr) {
      m_evp_key = CFGOpenSSL::read_private_key(filepath, passphrase, m_is_ec);
      m_is_private = true;
    }
    CFG_ASSERT(m_evp_key != nullptr);
  }
  if (m_is_ec) {
    char group_name[64] = {0};
    size_t group_name_size = 0;
    CFG_ASSERT(EVP_PKEY_get_group_name(get_evp_pkey(m_evp_key), group_name,
                                       sizeof(group_name),
                                       &group_name_size) == 1);
    m_key_info =
        CFGOpenSSL::get_key_info(std::string(group_name, group_name_size));
  } else {
    int rsa_bits = EVP_PKEY_get_bits(get_evp_pkey(m_evp_key));
    CFG_ASSERT(rsa_bits >= 512);
    CFG_ASSERT((rsa_bits % 256) == 0);
    CFG_ASSERT(rsa_bits <= 16384);
    m_key_info = CFGOpenSSL::get_key_info(NID_rsa, rsa_bits / 8);
  }
  CFG_ASSERT(m_key_info != nullptr);
  get_public_key();
}

void CFGCrypto_KEY::get_public_key() {
  CFG_ASSERT(m_evp_key != nullptr);
  CFG_ASSERT(m_key_info != nullptr);
  memset(m_public_key, 0, sizeof(m_public_key));
  uint8_t* ptr = &m_public_key[0];
  int len = i2d_PublicKey(get_evp_pkey(m_evp_key), &ptr);
  // Make sure m_public_key has enough memory space and no memory overwritten
  // happen
  CFG_ASSERT(len > 0 && ((size_t)(len) <= sizeof(m_public_key)));
#if ENABLE_DEBUG
  printf("Retrieved DER: %d\n ", len);
  for (uint32_t i = 0; i < uint32_t(len); i++) {
    printf(" %02X", m_public_key[i]);
    if ((i % 16) == 15) {
      printf("\n ");
    }
  }
  printf("\n");
#endif
  // Retrieve public key from binary
  if (m_is_ec) {
    CFG_ASSERT((size_t)(m_key_info->size * 2) <= sizeof(m_public_key));
    CFG_ASSERT((size_t)(m_key_info->size * 2) == size_t(len - 1));
    CFG_ASSERT(m_public_key[0] == 0x4);
    // Set the size
    m_public_key_size = 2 * m_key_info->size;
    // Move the data
    memcpy(m_public_key, &m_public_key[1], m_public_key_size);
    memset(&m_public_key[m_public_key_size], 0,
           sizeof(m_public_key) - size_t(m_public_key_size));
  } else {
    // RSA does not have x, y
    // It has public + 4 bytes length
    CFG_ASSERT((size_t)(m_key_info->size + 4) <= sizeof(m_public_key));
    // Need to interpret the DER
    uint32_t der_total_len = 0;
    size_t index = 0;
    uint8_t der_type = get_der_type_and_length(m_public_key, size_t(len), index,
                                               der_total_len);
    // Every DER should start with 0x30 type and the length should match the
    // total length
    CFG_ASSERT(der_type == 0x30);
    CFG_ASSERT(der_total_len);
    CFG_ASSERT((index + size_t(der_total_len)) == size_t(len));
    // 1st should be integer of public
    uint32_t pub_key_len = 0;
    der_type =
        get_der_type_and_length(m_public_key, size_t(len), index, pub_key_len);
    CFG_ASSERT(der_type == 0x02);
    CFG_ASSERT(pub_key_len);
    CFG_ASSERT((index + size_t(pub_key_len)) < size_t(len));
    // Even the maximum MSB is signed number, we will only add one zero to make
    // it unsigned
    CFG_ASSERT(pub_key_len <= (m_key_info->size + 1));
    size_t pub_key_index = index;
    index += size_t(pub_key_len);
    // 2nd should be integer of RSA set_words
    uint32_t size_len = 0;
    der_type =
        get_der_type_and_length(m_public_key, size_t(len), index, size_len);
    CFG_ASSERT(der_type == 0x02);
    CFG_ASSERT(size_len);
    CFG_ASSERT((index + size_t(size_len)) == size_t(len));
    // Our spec only reserve 4 bytes of the set_words
    uint32_t set_word_size = 0;
    CFG_ASSERT(sizeof(size_len) <= sizeof(set_word_size));
    for (size_t i = 0; i < size_len; i++) {
      set_word_size =
          (set_word_size << 8) | (uint32_t(m_public_key[index++]) & 0xFF);
    }
    CFG_ASSERT(index == size_t(len));
    // Deal the pub key now
    if (pub_key_len == (m_key_info->size + 1)) {
      CFG_ASSERT(m_public_key[pub_key_index] == 0);
      memcpy(m_public_key, &m_public_key[pub_key_index + 1], pub_key_len - 1);
    } else {
      CFG_ASSERT(m_public_key[pub_key_index] != 0);
      CFG_ASSERT((m_public_key[pub_key_index] & 0x80) == 0);
      memcpy(m_public_key, &m_public_key[pub_key_index], pub_key_len);
      if (pub_key_len < m_key_info->size) {
        // push back
        for (uint32_t i = 0, src_index = pub_key_len - 1,
                      dest_index = m_key_info->size - 1;
             i < m_key_info->size; i++, src_index--, dest_index--) {
          if (i >= pub_key_len) {
            m_public_key[dest_index] = 0;
          } else {
            m_public_key[dest_index] = m_public_key[src_index];
          }
        }
      }
      memset(&m_public_key[m_key_info->size], 0,
             sizeof(m_public_key) - size_t(m_key_info->size));
    }
    // backup set_words
    memcpy(&m_public_key[m_key_info->size], (uint8_t*)(&set_word_size),
           sizeof(set_word_size));
    m_public_key_size = m_key_info->size + sizeof(set_word_size);
#if ENABLE_DEBUG
    printf("Raw RSA Pub Key + Size: %d\n ", m_public_key_size);
    for (uint32_t i = 0; i < m_public_key_size; i++) {
      printf(" %02X", m_public_key[i]);
      if ((i % 16) == 15) {
        printf("\n ");
      }
    }
    printf("\n");
#endif
  }
}

void CFGCrypto_KEY::get_public_key(std::vector<uint8_t>& pubkey,
                                   uint32_t alignment) {
  pubkey.clear();
  CFG_ASSERT(m_key_info != nullptr);
  CFG_ASSERT(m_public_key_size);
  for (uint32_t i = 0; i < m_public_key_size; i++) {
    pubkey.push_back(m_public_key[i]);
  }
  if (alignment != 0) {
    while (pubkey.size() % alignment) {
      pubkey.push_back(0);
    }
  }
}

bool CFGCrypto_KEY::verify_signature(const uint8_t* digest,
                                     const size_t digest_size,
                                     const uint8_t* signature,
                                     const size_t signature_size) {
  CFG_ASSERT(digest != nullptr && digest_size > 0);
  CFG_ASSERT(signature != nullptr && signature_size > 0);
  bool status = false;
  CFG_ASSERT(m_evp_key != nullptr);
  CFG_ASSERT(m_key_info != nullptr);
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(get_evp_pkey(m_evp_key), nullptr);
  CFG_ASSERT(ctx != nullptr);
  CFG_ASSERT(EVP_PKEY_verify_init(ctx) == 1);
  if (m_is_ec) {
    CFG_ASSERT(m_key_info->size < 128);
    CFG_ASSERT(signature_size == get_signature_size());
    uint8_t temp[1024] = {0};
    // 0x30 + length
    //    0x02 + length + R
    //    0x02 + length + S
    // Prepare to support
    uint32_t xlen = get_der_compact_length(signature, m_key_info->size);
    uint32_t ylen =
        get_der_compact_length(&signature[m_key_info->size], m_key_info->size);
    uint32_t total_der_size = 4 + xlen + ylen;
    size_t index = 0;
    temp[index++] = 0x30;
    if (total_der_size >= 128) {
      temp[index++] = 0x82;
      temp[index++] = (total_der_size >> 8) & 0xFF;
      temp[index++] = (total_der_size)&0xFF;
    } else {
      temp[index++] = (uint8_t)(total_der_size);
    }
    temp[index++] = 0x02;
    temp[index++] = (uint8_t)(xlen);
    if (xlen > m_key_info->size) {
      temp[index++] = 0;
      xlen = m_key_info->size;
    }
    memcpy(&temp[index], &signature[m_key_info->size - xlen], xlen);
    index += (size_t)(xlen);
    temp[index++] = 0x02;
    temp[index++] = (uint8_t)(ylen);
    if (ylen > m_key_info->size) {
      temp[index++] = 0;
      ylen = m_key_info->size;
    }
    memcpy(&temp[index], &signature[m_key_info->size + m_key_info->size - ylen],
           ylen);
    index += (size_t)(ylen);
#if ENABLE_DEBUG
    printf("Constructed DER Signature: %ld\n ", index);
    for (size_t i = 0; i < index; i++) {
      printf(" %02X", temp[i]);
      if ((i % 16) == 15) {
        printf("\n ");
      }
    }
    printf("\n");
#endif
    status = EVP_PKEY_verify(ctx, temp, index, digest, digest_size) == 1;
    memset(temp, 0, sizeof(temp));
  } else {
    status = EVP_PKEY_verify(ctx, signature, signature_size, digest,
                             digest_size) == 1;
  }
  return status;
}

size_t CFGCrypto_KEY::sign(const uint8_t* digest, const size_t digest_size,
                           uint8_t* signature_data,
                           const size_t signature_size) {
  CFG_ASSERT(m_is_private);
  CFG_ASSERT(digest != nullptr && digest_size > 0);
  CFG_ASSERT(signature_data != nullptr && signature_size > 0);
  CFG_ASSERT(m_evp_key != nullptr);
  CFG_ASSERT(m_key_info != nullptr);
  uint8_t temp[1024] = {0};
  size_t signed_size = sizeof(temp);
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(get_evp_pkey(m_evp_key), nullptr);
  CFG_ASSERT(ctx != nullptr);
  CFG_ASSERT(EVP_PKEY_sign_init(ctx) == 1);
  CFG_ASSERT(EVP_PKEY_sign(ctx, temp, &signed_size, digest, digest_size) == 1);
#if ENABLE_DEBUG
  printf("Generated Signature: %ld\n ", signed_size);
  for (size_t i = 0; i < signed_size; i++) {
    printf(" %02X", temp[i]);
    if ((i % 16) == 15) {
      printf("\n ");
    }
  }
  printf("\n");
#endif
  if (m_is_ec) {
    CFG_ASSERT(signature_size >= size_t(m_key_info->size * 2));
    // Need to interpret the DER
    uint32_t der_total_len = 0;
    size_t index = 0;
    uint8_t der_type =
        get_der_type_and_length(temp, signed_size, index, der_total_len);
    // Every DER should start with 0x30 type and the length should match the
    // total length
    CFG_ASSERT(der_type == 0x30);
    CFG_ASSERT(der_total_len);
    CFG_ASSERT((index + size_t(der_total_len)) == signed_size);
    // Loop twice for X and Y
    memset(signature_data, 0, m_key_info->size * 2);
    for (int i = 0; i < 2; i++) {
      uint32_t xylen = 0;
      der_type = get_der_type_and_length(temp, signed_size, index, xylen);
      CFG_ASSERT(der_type == 0x02);
      CFG_ASSERT(xylen);
      if (i == 0) {
        CFG_ASSERT((index + size_t(xylen)) < signed_size);
      } else {
        CFG_ASSERT((index + size_t(xylen)) == signed_size);
      }
      // Even the maximum MSB is signed number, we will only add one zero to
      // make it unsigned
      CFG_ASSERT(xylen <= (m_key_info->size + 1));
      if (xylen == (m_key_info->size + 1)) {
        CFG_ASSERT(temp[index++] == 0);
        memcpy(&signature_data[i * m_key_info->size], &temp[index],
               m_key_info->size);
        index += (size_t)(m_key_info->size);
      } else {
        memcpy(&signature_data[((i + 1) * m_key_info->size) - xylen],
               &temp[index], xylen);
        index += (size_t)(xylen);
      }
    }
    CFG_ASSERT(index == signed_size);
    signed_size = (size_t)(get_signature_size());
  } else {
    CFG_ASSERT(signed_size == (size_t)(get_signature_size()));
    CFG_ASSERT(signature_size >= signed_size);
    memcpy(signature_data, temp, signed_size);
  }
  CFG_ASSERT(
      verify_signature(digest, digest_size, signature_data, signed_size));
  EVP_PKEY_CTX_free(ctx);
  memset(temp, 0, sizeof(temp));
  return signed_size;
}
