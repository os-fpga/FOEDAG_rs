#include "CFGCrypto_key.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#define ENABLE_DEBUG 0

static EVP_PKEY* get_evp_pkey(const void* key) {
  return reinterpret_cast<EVP_PKEY*>(const_cast<void*>(key));
}

static uint8_t get_der_type_and_length(const uint8_t* data, size_t data_size,
                                       size_t& index, uint32_t& len) {
  CFG_ASSERT(data != nullptr && data_size > 0);
  CFG_ASSERT(index < data_size);
  uint8_t type = data[index++];
  CFG_ASSERT(index < data_size);
  len = (uint32_t)(data[index++]) & 0xFF;
  if (len & 0x80) {
    uint8_t len_size = len & 0x7F;
    // The len reference is uint32_t, so maximum len_size should be 4
    CFG_ASSERT(len_size > 0 && len_size <= 4);
    len = 0;
    for (uint8_t i = 0; i < len_size; i++) {
      CFG_ASSERT(index < data_size);
      len = (len << 8) | (uint32_t(data[index++]) & 0xFF);
    }
    CFG_ASSERT(len > 127);  // If it is 0 - 127, then we only need one byte
  }
  // The length must make sense that won't exceed data size
  CFG_ASSERT((index + len) <= data_size);
  return type;
}

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
    CFG_ASSERT(pub_key_size == (2 * m_key_info->size));
    BIGNUM* x = BN_bin2bn(pub_key, m_key_info->size, NULL);
    BIGNUM* y = BN_bin2bn(&pub_key[m_key_info->size], m_key_info->size, NULL);
    CFG_ASSERT(x != NULL && y != NULL)
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(m_key_info->nid);
    CFG_ASSERT(EC_KEY_set_public_key_affine_coordinates(ec_key, x, y) == 1);
    CFG_ASSERT(EC_KEY_check_key(ec_key) == 1);
    CFG_ASSERT(EC_KEY_get0_public_key(ec_key) != NULL);
    m_evp_key = EVP_PKEY_new();
    CFG_ASSERT(m_evp_key != nullptr);
    CFG_ASSERT(EVP_PKEY_assign(get_evp_pkey(m_evp_key),
                               NID_X9_62_id_ecPublicKey, ec_key) == 1);
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
    RSA* rsa_key =
        d2i_RSAPublicKey(nullptr, (const uint8_t**)(&ptr), long(index));
    CFG_ASSERT(rsa_key != nullptr);
    m_evp_key = EVP_PKEY_new();
    CFG_ASSERT(m_evp_key != nullptr);
    CFG_ASSERT(EVP_PKEY_assign(get_evp_pkey(m_evp_key), NID_rsaEncryption,
                               rsa_key) == 1);
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
  // Make sure the real key (not just key type) is supported
  if (m_is_ec) {
    EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(get_evp_pkey(m_evp_key));
    int nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key));
    m_key_info = CFGOpenSSL::get_key_info(nid, 0);
  } else {
    RSA* rsa_key = EVP_PKEY_get0_RSA(get_evp_pkey(m_evp_key));
    m_key_info = CFGOpenSSL::get_key_info(NID_rsa, RSA_size(rsa_key));
  }
  CFG_ASSERT(m_key_info != nullptr);
  get_public_key();
}

void CFGCrypto_KEY::get_public_key() {
  CFG_ASSERT(m_evp_key != nullptr);
  CFG_ASSERT(m_key_info != nullptr);
  memset(m_public_key, 0, sizeof(m_public_key));
  if (m_is_ec) {
    CFG_ASSERT((size_t)(m_key_info->size * 2) <= sizeof(m_public_key));
    EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(get_evp_pkey(m_evp_key));
    const EC_POINT* pub = EC_KEY_get0_public_key(ec_key);
    BIGNUM* x = BN_new();
    BIGNUM* y = BN_new();
    CFG_ASSERT(EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec_key),
                                                   pub, x, y, nullptr));

    // X
    uint32_t len = uint32_t(BN_num_bytes(x));
    CFG_ASSERT(len <= m_key_info->size);
    CFG_ASSERT(uint32_t(BN_bn2bin(x, &m_public_key[m_key_info->size - len])) ==
               len);

    // Y
    len = uint32_t(BN_num_bytes(y));
    CFG_ASSERT(len <= m_key_info->size);
    CFG_ASSERT(uint32_t(BN_bn2bin(
                   y, &m_public_key[2 * m_key_info->size - len])) == len);

    // Free memory
    BN_clear_free(x);
    BN_clear_free(y);

    // Set the size
    m_public_key_size = 2 * m_key_info->size;
  } else {
    // RSA does not have x, y
    // It has public + 4 bytes length
    CFG_ASSERT((size_t)(m_key_info->size + 4) <= sizeof(m_public_key));
    RSA* rsa_key = EVP_PKEY_get0_RSA(get_evp_pkey(m_evp_key));
    // The information is stored in DER
    uint8_t* ptr = &m_public_key[0];
    int len = i2d_RSAPublicKey(rsa_key, &ptr);
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
  if (m_is_ec) {
    CFG_ASSERT(signature_size == (size_t)(2 * m_key_info->size));
    EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(get_evp_pkey(m_evp_key));
    BIGNUM* r = BN_bin2bn(signature, m_key_info->size, nullptr);
    BIGNUM* s =
        BN_bin2bn(&signature[m_key_info->size], m_key_info->size, nullptr);
    CFG_ASSERT(r != nullptr && s != nullptr);
    ECDSA_SIG* ec_sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(ec_sig, r, s);
    status = ECDSA_do_verify(digest, digest_size, ec_sig, ec_key) == 1;
    ECDSA_SIG_free(ec_sig);
  } else {
    RSA* rsa_key = EVP_PKEY_get0_RSA(get_evp_pkey(m_evp_key));
    status = RSA_verify(m_key_info->digest_nid, digest, digest_size, signature,
                        signature_size, rsa_key) == 1;
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
  size_t signed_size = 0;
  if (m_is_ec) {
    signed_size = (size_t)(2 * m_key_info->size);
    CFG_ASSERT(signature_size >= signed_size);
    EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(get_evp_pkey(m_evp_key));
    ECDSA_SIG* signature = ECDSA_do_sign(digest, digest_size, ec_key);
    CFG_ASSERT(signature != nullptr);
    CFG_ASSERT(ECDSA_do_verify(digest, digest_size, signature, ec_key) == 1);
    memset(signature_data, 0, signed_size);
    // Get RS
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(signature, &r, &s);
    // R
    uint32_t len = uint32_t(BN_num_bytes(r));
    CFG_ASSERT(len <= m_key_info->size);
    CFG_ASSERT(
        uint32_t(BN_bn2bin(r, &signature_data[m_key_info->size - len])) == len);
    // S
    len = uint32_t(BN_num_bytes(s));
    CFG_ASSERT(len <= m_key_info->size);
    CFG_ASSERT(uint32_t(BN_bn2bin(
                   s, &signature_data[2 * m_key_info->size - len])) == len);
    // Free memory
    ECDSA_SIG_free(signature);
  } else {
    CFG_ASSERT(signature_size >= (size_t)(m_key_info->size));
    RSA* rsa_key = EVP_PKEY_get0_RSA(get_evp_pkey(m_evp_key));
    CFG_ASSERT(RSA_sign(m_key_info->digest_nid, digest, digest_size,
                        signature_data, (unsigned int*)(&signed_size),
                        rsa_key) == 1);
    CFG_ASSERT(signed_size == (size_t)(m_key_info->size));
    CFG_ASSERT(RSA_verify(m_key_info->digest_nid, digest, digest_size,
                          signature_data, signed_size, rsa_key) == 1);
  }
  return signed_size;
}

uint32_t CFGCrypto_KEY::get_bitstream_signing_algo() {
  CFG_ASSERT(m_key_info != nullptr);
  return m_key_info->bitstream_algo;
}

uint32_t CFGCrypto_KEY::get_public_key_size(uint32_t alignment) {
  CFG_ASSERT(m_key_info != nullptr);
  CFG_ASSERT(m_public_key_size);
  if (alignment == 0) {
    return (m_public_key_size);
  } else {
    uint32_t size = m_public_key_size;
    while (size % alignment) {
      size++;
    }
    return size;
  }
}

uint32_t CFGCrypto_KEY::get_signature_size() {
  CFG_ASSERT(m_key_info != nullptr);
  if (m_is_ec) {
    return (2 * m_key_info->size);
  } else {
    return (m_key_info->size);
  }
}

size_t CFGCrypto_KEY::get_digest(const uint8_t* message,
                                 const size_t message_size, uint8_t* digest,
                                 const size_t digest_size) {
  CFG_ASSERT(m_key_info != nullptr);
  if (m_key_info->digest_nid == NID_sha256) {
    CFG_ASSERT(digest_size >= m_key_info->digest_size);
    CFGOpenSSL::sha_256(message, message_size, digest);
  } else if (m_key_info->digest_nid == NID_sha384) {
    CFG_ASSERT(digest_size >= m_key_info->digest_size);
    CFGOpenSSL::sha_384(message, message_size, digest);
  } else if (m_key_info->digest_nid == NID_sha512) {
    CFG_ASSERT(digest_size >= m_key_info->digest_size);
    CFGOpenSSL::sha_512(message, message_size, digest);
  } else {
    CFG_INTERNAL_ERROR("Unsupported digest NID %d", m_key_info->digest_nid);
  }
  return m_key_info->digest_size;
}
