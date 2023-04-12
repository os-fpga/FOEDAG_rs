#include "openssl/param_build.h"

static EVP_PKEY* generate_ec_key(const std::string& curvename) {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  EVP_PKEY* key = EVP_PKEY_new();
  CFG_ASSERT(EVP_PKEY_keygen_init(ctx) == 1);
  CFG_ASSERT(EVP_PKEY_CTX_set_group_name(ctx, curvename.c_str()) == 1);
  CFG_ASSERT(EVP_PKEY_keygen(ctx, &key) == 1);
  EVP_PKEY_CTX_free(ctx);
  return key;
}

static EVP_PKEY* generate_rsa_key(uint32_t bits) {
  // Minimum 512 bits
  // Multiple of 256 bits
  CFG_ASSERT(bits >= 512);
  CFG_ASSERT((bits % 256) == 0);
  CFG_ASSERT(bits <= 16384);
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  EVP_PKEY* key = EVP_PKEY_new();
  CFG_ASSERT(EVP_PKEY_keygen_init(ctx) == 1);
  CFG_ASSERT(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, int(bits)) == 1);
  CFG_ASSERT(EVP_PKEY_keygen(ctx, &key) == 1);
  EVP_PKEY_CTX_free(ctx);
  return key;
}

void CFGOpenSSL::ctr_encrypt(const uint8_t* plain_data, uint8_t* cipher_data,
                             size_t data_size, uint8_t* key, size_t key_size,
                             uint8_t* iv, size_t iv_size) {
  CFG_ASSERT(plain_data != nullptr);
  CFG_ASSERT(cipher_data != nullptr);
  CFG_ASSERT(data_size > 0);
  CFG_ASSERT(key != nullptr);
  CFG_ASSERT(key_size == 16 || key_size == 32);
  CFG_ASSERT(iv != nullptr);
  CFG_ASSERT(iv_size == 16);
  init_openssl();

  // encrypt
  // Initialize the OpenSSL crypto context
  int cipher_size = 0;
  const EVP_CIPHER* cipher =
      key_size == 16 ? EVP_aes_128_ctr() : EVP_aes_256_ctr();
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  CFG_ASSERT(ctx != nullptr);
  CFG_ASSERT(EVP_EncryptInit(ctx, cipher, key, iv) == 1);
  CFG_ASSERT(EVP_EncryptUpdate(ctx, cipher_data, &cipher_size, plain_data,
                               int(data_size)) == 1);
  CFG_ASSERT(cipher_size == int(data_size));
  // To CTR this seems to be redundant
  CFG_ASSERT(EVP_EncryptFinal(ctx, cipher_data, &cipher_size) == 1);
  CFG_ASSERT(cipher_size == 0);
  EVP_CIPHER_CTX_free(ctx);
}

void CFGOpenSSL::gen_private_pem(const std::string& key_type,
                                 const std::string& filepath,
                                 const std::string& passphrase,
                                 const bool skip_passphrase) {
  init_openssl();
  const CFGOpenSSL_KEY_INFO* key_info = get_key_info(key_type);
  EVP_PKEY* key = nullptr;
  if (key_info->evp_pkey_id == NID_X9_62_id_ecPublicKey) {
    key = generate_ec_key(key_type);
  } else {
    key = generate_rsa_key(key_info->size * 8);
  }
  BIO* bio = BIO_new(BIO_s_file());
  CFG_ASSERT(bio != nullptr);
  int status = BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_WRITE,
                        const_cast<char*>(filepath.c_str()));
  CFG_ASSERT_MSG(status > 0, "Fail to open file %s for BIO write",
                 filepath.c_str());
  if (skip_passphrase) {
    CFG_ASSERT(PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr,
                                        nullptr) == 1);
    EVP_PKEY_free(key);
  } else {
    std::string final_passphrase = "";
    get_passphrase(final_passphrase, passphrase, MIN_PASSPHRASE_SIZE);
    if (final_passphrase.size() == 0) {
      final_passphrase.resize(1024);
      memset(const_cast<char*>(final_passphrase.c_str()), 0,
             final_passphrase.size());
      size_t password_size = (size_t)(PASSPHRASE_CALLBACK(
          const_cast<char*>(final_passphrase.c_str()), final_passphrase.size(),
          true,
          const_cast<char*>(
              CFG_print("Need passphrase to encrypt %s", filepath.c_str())
                  .c_str())));
      CFG_ASSERT(password_size >= MIN_PASSPHRASE_SIZE &&
                 password_size <= final_passphrase.size());
      final_passphrase.resize(password_size);
    }
    // Convert EC Key to EVP Key
    PKCS8_PRIV_KEY_INFO* p8inf = nullptr;
    X509_ALGOR* pbe = nullptr;
    X509_SIG* p8 = nullptr;
    if (status <= 0) {
      memory_clean(final_passphrase, key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to assign EC Key to EVP Key");
    }
    // Convert EVP Key to PKCS8
    p8inf = EVP_PKEY2PKCS8(key);
    if (p8inf == nullptr) {
      memory_clean(final_passphrase, key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to convert EVP Key to PKCS8");
    }
    // Create PBE algorithm from ass256-cbc EVP_CIPHER and pbe_nid
    pbe = PKCS5_pbe2_set_iv(EVP_aes_256_cbc(), 5000000, nullptr, 0, nullptr,
                            NID_hmacWithSHA512);
    if (pbe == nullptr) {
      memory_clean(final_passphrase, key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to create PBE X509_ALGOR");
    }
    // Encrypt
    p8 = PKCS8_set0_pbe(final_passphrase.c_str(), int(final_passphrase.size()),
                        p8inf, pbe);
    if (p8 == nullptr) {
      memory_clean(final_passphrase, key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to create PKCS8 X509_SIG");
    }
    // Write to BIO
    status = PEM_write_bio_PKCS8(bio, p8);
    if (status <= 0) {
      memory_clean(final_passphrase, key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to write X509_SIG to PEM BIO");
    }
    // Free
    memory_clean(final_passphrase, key, p8inf, p8);
  }
  BIO_free_all(bio);
}

void CFGOpenSSL::gen_public_pem(const std::string& private_filepath,
                                const std::string& public_filepath,
                                const std::string& passphrase) {
  init_openssl();
  bool is_ec = false;
  EVP_PKEY* key =
      (EVP_PKEY*)(read_private_key(private_filepath, passphrase, is_ec));
  EVP_PKEY* pubkey = NULL;
  BIO* bio = BIO_new(BIO_s_file());
  int status = BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_WRITE,
                        const_cast<char*>(public_filepath.c_str()));
  CFG_ASSERT_MSG(status > 0, "Fail to open file %s for BIO write",
                 public_filepath.c_str());
  OSSL_PARAM* params = NULL;
  CFG_ASSERT(EVP_PKEY_todata(key, EVP_PKEY_PUBLIC_KEY, &params) == 1);
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
  CFG_ASSERT(ctx != nullptr);
  CFG_ASSERT(EVP_PKEY_fromdata_init(ctx) == 1);
  CFG_ASSERT(EVP_PKEY_fromdata(ctx, &pubkey, EVP_PKEY_PUBLIC_KEY, params) == 1);
  CFG_ASSERT(PEM_write_bio_PUBKEY(bio, pubkey) == 1);
  // clean up
  EVP_PKEY_free(pubkey);
  EVP_PKEY_CTX_free(ctx);
  OSSL_PARAM_free(params);
  BIO_free_all(bio);
  EVP_PKEY_free(key);
}
