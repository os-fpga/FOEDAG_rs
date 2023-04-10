#ifndef CFGCrypto_KEY_H
#define CFGCrypto_KEY_H

#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGOpenSSL.h"

class CFGCrypto_KEY {
 public:
  CFGCrypto_KEY();
  CFGCrypto_KEY(const std::string& filepath, const std::string& passphrase,
                bool expected_private);
  CFGCrypto_KEY(const std::string& key_type, const uint8_t* pub_key,
                const uint32_t pub_key_size);
  ~CFGCrypto_KEY();
  void initial(const std::string& filepath, const std::string& passphrase,
               bool expected_private);
  size_t get_digest(const uint8_t* message, const size_t message_size,
                    uint8_t* digest, const size_t digest_size);
  bool verify_signature(const uint8_t* digest, const size_t digest_size,
                        const uint8_t* signature, const size_t signature_size);
  size_t sign(const uint8_t* digest, const size_t digest_size,
              uint8_t* signature_data, const size_t signature_size);
  uint32_t get_bitstream_signing_algo();
  uint32_t get_public_key_size(uint32_t alignment);
  uint32_t get_signature_size();
  void get_public_key(std::vector<uint8_t>& pubkey, uint32_t alignment);

 private:
  void get_public_key();
  uint8_t get_der_type_and_length(const uint8_t* data, size_t data_size,
                                  size_t& index, uint32_t& len);
  std::string m_filepath = "";
  const void* m_evp_key = nullptr;
  const CFGOpenSSL_KEY_INFO* m_key_info = nullptr;
  bool m_is_private = false;
  bool m_is_ec = false;
  int m_nid = 0;
  // allocate for RSA4096 even we do not support now
  uint32_t m_public_key_size = 0;
  uint8_t m_public_key[1024];
};

#endif
