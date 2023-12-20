#ifndef CFGOpenSSL_H
#define CFGOpenSSL_H

#include "CFGCommonRS/CFGCommonRS.h"
#define APP_OPENSSL_MAJOR_VERSION (1)
#define APP_OPENSSL_MINOR_VERSION (1)
#define APP_USE_1_1_x_OPENSSL (1)
#define OPENSSL_API_COMPAT \
  (APP_OPENSSL_MAJOR_VERSION * 10000 + APP_OPENSSL_MINOR_VERSION * 100)

struct CFGOpenSSL_KEY_INFO {
  CFGOpenSSL_KEY_INFO(int ni, int e, const std::string& n, uint32_t s, int d,
                      size_t ds, uint8_t b)
      : nid(ni),
        evp_pkey_id(e),
        name(n),
        size(s),
        digest_nid(d),
        digest_size(ds),
        bitstream_algo(b) {}
  const int nid;
  const int evp_pkey_id;
  const std::string name;
  const uint32_t size;
  const int digest_nid;
  const size_t digest_size;
  const uint8_t bitstream_algo;
};

class CFGCrypto_KEY;

class CFGOpenSSL {
 public:
  static void init_openssl();
  static void sha_256(const uint8_t* data, size_t data_size, uint8_t* sha);
  static void sha_384(const uint8_t* data, size_t data_size, uint8_t* sha);
  static void sha_512(const uint8_t* data, size_t data_size, uint8_t* sha);
  static void sha(uint8_t hash_size, const uint8_t* data, size_t data_size,
                  uint8_t* sha);
  static void ctr_encrypt(const uint8_t* plain_data, uint8_t* cipher_data,
                          size_t data_size, uint8_t* key, size_t key_size,
                          const uint8_t* iv, size_t iv_size,
                          uint8_t* returned_iv = nullptr);
  static void ctr_decrypt(const uint8_t* cipher_data, uint8_t* plain_data,
                          size_t data_size, uint8_t* key, size_t key_size,
                          const uint8_t* iv, size_t iv_size,
                          uint8_t* returned_iv = nullptr);
  static void gen_private_pem(const std::string& key_type,
                              const std::string& filepath,
                              const std::string& passphrase,
                              const bool skip_passphrase);
  static void gen_public_pem(const std::string& private_filepath,
                             const std::string& public_filepath,
                             const std::string& passphrase);
  static void* read_private_key(const std::string& filepath,
                                const std::string& passphrase, bool& is_ec);
  static void* read_public_key(const std::string& filepath,
                               const std::string& passphrase, bool& is_ec,
                               const bool allow_failure = false);
  static bool authenticate_message(const uint8_t* message,
                                   const size_t message_size,
                                   const uint8_t* signature,
                                   const size_t signature_size,
                                   const std::string& key_type,
                                   const uint8_t* pub_key,
                                   const uint32_t pub_key_size);
  static bool authenticate_message(const uint8_t* message,
                                   const size_t message_size,
                                   const uint8_t* signature,
                                   const size_t signature_size,
                                   const std::string& filepath,
                                   const std::string& passphrase);
  static bool authenticate_message(const uint8_t* message,
                                   const size_t message_size,
                                   const uint8_t* signature,
                                   const size_t signature_size,
                                   CFGCrypto_KEY* key);
  static bool authenticate_digest(
      const uint8_t* digest, const size_t digest_size, const uint8_t* signature,
      const size_t signature_size, const std::string& key_type,
      const uint8_t* pub_key, const uint32_t pub_key_size);
  static bool authenticate_digest(const uint8_t* digest,
                                  const size_t digest_size,
                                  const uint8_t* signature,
                                  const size_t signature_size,
                                  const std::string& filepath,
                                  const std::string& passphrase);
  static bool authenticate_digest(const uint8_t* digest,
                                  const size_t digest_size,
                                  const uint8_t* signature,
                                  const size_t signature_size,
                                  CFGCrypto_KEY* key);
  static size_t sign_message(const uint8_t* message, const size_t message_size,
                             uint8_t* signature, const size_t signature_size,
                             const std::string& filepath,
                             const std::string& passphrase);
  static size_t sign_message(const uint8_t* message, const size_t message_size,
                             uint8_t* signature, const size_t signature_size,
                             CFGCrypto_KEY* key);
  static size_t sign_digest(const uint8_t* digest, const size_t digest_size,
                            uint8_t* signature, const size_t signature_size,
                            const std::string& filepath,
                            const std::string& passphrase);
  static size_t sign_digest(const uint8_t* digest, const size_t digest_size,
                            uint8_t* signature, const size_t signature_size,
                            CFGCrypto_KEY* key);
  static const CFGOpenSSL_KEY_INFO* get_key_info(int nid, int size);
  static const CFGOpenSSL_KEY_INFO* get_key_info(const std::string& key_name);
  static void generate_random_data(uint8_t* data, const size_t data_size);
  static uint32_t generate_random_u32();
  static uint64_t generate_random_u64();
  static void generate_iv(uint8_t* iv, bool gcm);

 private:
  static void get_passphrase(std::string& final_passphrase,
                             const std::string& passphrase,
                             const uint32_t min_passphrase);
};

#endif
