#include "CFGOpenSSL.h"

#include <fstream>
#include <streambuf>
#include <string>

#include "CFGCrypto_key.h"
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "openssl/configuration.h"
#include "openssl/aes.h"
#include "openssl/bio.h"
#include "openssl/crypto.h"
#include "openssl/ec.h"
#include "openssl/evp.h"
#include "openssl/modes.h"
#include "openssl/pem.h"
#include "openssl/pkcs12.h"
#include "openssl/rand.h"
#include "openssl/sha.h"

#define MIN_PASSPHRASE_SIZE (13)
const std::vector<CFGOpenSSL_KEY_INFO> CFGOpenSSL_KEY_INFO_DATABASE = {
    CFGOpenSSL_KEY_INFO(NID_X9_62_prime256v1, NID_X9_62_id_ecPublicKey,
                        "prime256v1", 32, NID_sha256, 32, 0x10),
    CFGOpenSSL_KEY_INFO(NID_rsa, NID_rsaEncryption, "rsa2048", 256, NID_sha256,
                        32, 0x20)};

static bool m_openssl_init = false;

static void memory_clean(std::string& passphrase, EVP_PKEY*& evp_key,
                         PKCS8_PRIV_KEY_INFO*& p8inf, X509_SIG*& p8) {
  memset(const_cast<char*>(passphrase.c_str()), 0, passphrase.size());
  if (p8 != nullptr) {
    X509_SIG_free(p8);
  }
  if (p8inf != nullptr) {
    PKCS8_PRIV_KEY_INFO_free(p8inf);
  }
  if (evp_key != nullptr) {
    EVP_PKEY_free(evp_key);
  }
}

static void SetStdinEcho(bool enable) {
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  uint32_t mode;
  GetConsoleMode(hStdin, (LPDWORD)(&mode));
  if (enable) {
    mode |= ENABLE_ECHO_INPUT;
  } else {
    mode &= ~ENABLE_ECHO_INPUT;
  }
  SetConsoleMode(hStdin, mode);
#else
  struct termios tty;
  tcgetattr(STDIN_FILENO, &tty);
  if (enable) {
    tty.c_lflag |= ECHO;
  } else {
    tty.c_lflag &= ~ECHO;
  }
  (void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

static int PASSPHRASE_CALLBACK(char* buf, int max_len, int flag, void* pwd) {
  if (pwd != nullptr) {
    char* msg = (char*)(pwd);
    CFG_POST_MSG(msg);
  }
  std::string pass = "";
  if (flag) {
    // This is used to encryption
    CFG_POST_MSG("Enter passphrase with minimum of %d characters:",
                 MIN_PASSPHRASE_SIZE);
  } else {
    // This is used to decryption
    CFG_POST_MSG("Enter passphrase:");
  }
  SetStdinEcho(false);
  std::cin >> pass;
  SetStdinEcho(true);
  if (flag > 0 && pass.size() < MIN_PASSPHRASE_SIZE) {
    // Clear the passphrase before assertion
    memset(const_cast<char*>(pass.c_str()), 0, pass.size());
    CFG_INTERNAL_ERROR(
        "Minimum passphrase size is %d, only %d character(s) is specified",
        MIN_PASSPHRASE_SIZE, pass.size());
  }
  if (flag > 0) {
    CFG_POST_MSG("Re-enter passphrase to confirm:");
    std::string re_pass;
    SetStdinEcho(false);
    std::cin >> re_pass;
    SetStdinEcho(true);
    if (pass != re_pass) {
      memset(const_cast<char*>(pass.c_str()), 0, pass.size());
      memset(const_cast<char*>(re_pass.c_str()), 0, re_pass.size());
      CFG_INTERNAL_ERROR("The two passphrases do not match");
    }
    memset(const_cast<char*>(re_pass.c_str()), 0, re_pass.size());
  }
  if (pass.size() == 0) {
    CFG_INTERNAL_ERROR("No passphrase is specified");
  }
  if (pass.size() > (size_t)(max_len)) {
    memset(const_cast<char*>(pass.c_str()), 0, pass.size());
    CFG_INTERNAL_ERROR(
        "Caller to PASSPHRASE_CALLBACK does not provide enough memory "
        "allocation (%d Bytes) to store passphrase (specified %d Bytes)",
        max_len, pass.size());
  }
  memcpy(buf, pass.c_str(), pass.size());
  memset(const_cast<char*>(pass.c_str()), 0, pass.size());
  return int(pass.size());
}

static EVP_PKEY* generate_ec_key(int nid) {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  EVP_PKEY* key = EVP_PKEY_new();
  CFG_ASSERT(EVP_PKEY_keygen_init(ctx) == 1);
  CFG_ASSERT(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, nid) == 1);
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

void CFGOpenSSL::init_openssl() {
  if (!m_openssl_init) {
    // CFG_POST_MSG("OpenSSL Version: %s\n", OpenSSL_version(0));
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS |
                            OPENSSL_INIT_ADD_ALL_DIGESTS |
                            OPENSSL_INIT_LOAD_CONFIG,
                        nullptr);
    m_openssl_init = true;
  }
}

void CFGOpenSSL::sha_256(const uint8_t* data, size_t data_size, uint8_t* sha) {
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(data_size > 0);
  init_openssl();
  SHA256(data, data_size, sha);
}

void CFGOpenSSL::sha_384(const uint8_t* data, size_t data_size, uint8_t* sha) {
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(data_size > 0);
  init_openssl();
  SHA384(data, data_size, sha);
}

void CFGOpenSSL::sha_512(const uint8_t* data, size_t data_size, uint8_t* sha) {
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(data_size > 0);
  init_openssl();
  SHA512(data, data_size, sha);
}

void CFGOpenSSL::sha(uint8_t hash_size, const uint8_t* data, size_t data_size,
                     uint8_t* sha) {
  CFG_ASSERT(hash_size == 32 || hash_size == 48 || hash_size == 64);
  if (hash_size == 32) {
    sha_256(data, data_size, sha);
  } else if (hash_size == 48) {
    sha_384(data, data_size, sha);
  } else {
    sha_512(data, data_size, sha);
  }
}

void CFGOpenSSL::ctr_encrypt(const uint8_t* plain_data, uint8_t* cipher_data,
                             size_t data_size, uint8_t* key, size_t key_size,
                             const uint8_t* iv, size_t iv_size,
                             uint8_t* returned_iv) {
  CFG_ASSERT(plain_data != nullptr);
  CFG_ASSERT(cipher_data != nullptr);
  CFG_ASSERT(data_size > 0);
  CFG_ASSERT(key != nullptr);
  CFG_ASSERT(key_size == 16 || key_size == 32);
  CFG_ASSERT(iv != nullptr);
  CFG_ASSERT(iv_size == 16);
  init_openssl();

  // encrypt
#if APP_USE_1_1_x_OPENSSL
  AES_KEY aes_key;
  unsigned char ecount_buf[16] = {0};
  unsigned char internal_iv[16] = {0};
  unsigned int aes_num(0);
  CFG_ASSERT(sizeof(internal_iv) == iv_size);
  memcpy(internal_iv, iv, sizeof(internal_iv));
  CFG_ASSERT(AES_set_encrypt_key(key, key_size * 8, &aes_key) == 0);
  CRYPTO_ctr128_encrypt(plain_data, cipher_data, data_size, &aes_key,
                        internal_iv, ecount_buf, &aes_num,
                        (block128_f)AES_encrypt);
  if (returned_iv != nullptr) {
    memcpy(returned_iv, internal_iv, iv_size);
  }
  memset(internal_iv, 0, sizeof(internal_iv));
#else
  // This piece of code is written (and tested working) which is compatible with 3.0
  EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
  CFG_ASSERT(ctx != nullptr);
  unsigned char internal_iv[16] = {0};
  CFG_ASSERT(sizeof(internal_iv) == iv_size);
  memcpy(internal_iv, iv, sizeof(internal_iv));
  // Init
  if (key_size == 16) {
    CFG_ASSERT(EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, key, iv));
  } else {
    CFG_ASSERT(EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv));
  }
  // Update (should be able to check overflow if the data_size is too big
  int cipher_data_size = 0;
  CFG_ASSERT(EVP_EncryptUpdate(ctx, cipher_data, &cipher_data_size, plain_data, int(data_size)));
  CFG_ASSERT((size_t)(cipher_data_size) == data_size);
  // Finalize
  // Pernally do not think CTR has any final size (unlike GCM)
  // Hence make sure final_data_size == 0
  int final_data_size = 0;
  unsigned char final_dummy[128] = { 0 };
  CFG_ASSERT(EVP_EncryptFinal_ex(ctx, final_dummy, &final_data_size));
  CFG_ASSERT(final_data_size == 0);
  if (returned_iv != nullptr) {
    memcpy(returned_iv, internal_iv, iv_size);
  }
  memset(internal_iv, 0, sizeof(internal_iv));
  EVP_CIPHER_CTX_free(ctx);
#endif
}

void CFGOpenSSL::gen_private_pem(const std::string& key_type,
                                 const std::string& filepath,
                                 const std::string& passphrase,
                                 const bool skip_passphrase) {
  init_openssl();
  EVP_PKEY* key = nullptr;
  const CFGOpenSSL_KEY_INFO* key_info = get_key_info(key_type);
  if (key_info->evp_pkey_id == NID_X9_62_id_ecPublicKey) {
    key = generate_ec_key(key_info->nid);
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
  BIO* bio = BIO_new(BIO_s_file());
  int status = BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_WRITE,
                        const_cast<char*>(public_filepath.c_str()));
  CFG_ASSERT_MSG(status > 0, "Fail to open file %s for BIO write",
                 public_filepath.c_str());
  // Currently only support two key type
  if (is_ec) {
    CFG_POST_MSG("Detected private EC Key");
    const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key);
    status = PEM_write_bio_EC_PUBKEY(bio, ec_key);
    CFG_ASSERT_MSG(status > 0, "Fail to write EC Public Key to PEM BIO");
  } else {
    CFG_POST_MSG("Detected private RSA Key");
    status = PEM_write_bio_PUBKEY(bio, key);
    CFG_ASSERT_MSG(status > 0, "Fail to write RSA Public Key to PEM BIO");
  }
  EVP_PKEY_free(key);
  BIO_free_all(bio);
  bool verify_is_ec = false;
  key = (EVP_PKEY*)(read_public_key(public_filepath, "", verify_is_ec));
  CFG_ASSERT(key != nullptr);
  EVP_PKEY_free(key);
  CFG_ASSERT(is_ec == verify_is_ec);
}

void CFGOpenSSL::ctr_decrypt(const uint8_t* cipher_data, uint8_t* plain_data,
                             size_t data_size, uint8_t* key, size_t key_size,
                             const uint8_t* iv, size_t iv_size,
                             uint8_t* returned_iv) {
  ctr_encrypt(cipher_data, plain_data, data_size, key, key_size, iv, iv_size,
              returned_iv);
}

const CFGOpenSSL_KEY_INFO* CFGOpenSSL::get_key_info(int nid, int size) {
  const CFGOpenSSL_KEY_INFO* key_info = nullptr;
  for (auto& c : CFGOpenSSL_KEY_INFO_DATABASE) {
    if (c.nid == nid) {
      if (c.nid == NID_rsa) {
        if (c.size != (uint32_t)(size)) {
          // For RSA, we do not have specific NID for different size
          // So further check the size
          continue;
        }
      }
      key_info = &c;
      break;
    }
  }
  CFG_ASSERT(key_info != nullptr);
  return key_info;
}

const CFGOpenSSL_KEY_INFO* CFGOpenSSL::get_key_info(
    const std::string& key_name) {
  const CFGOpenSSL_KEY_INFO* key_info = nullptr;
  for (auto& c : CFGOpenSSL_KEY_INFO_DATABASE) {
    if (c.name == key_name) {
      key_info = &c;
      break;
    }
  }
  CFG_ASSERT(key_info != nullptr);
  return key_info;
}

void* CFGOpenSSL::read_private_key(const std::string& filepath,
                                   const std::string& passphrase, bool& is_ec) {
  init_openssl();
  std::string final_passphrase = "";
  get_passphrase(final_passphrase, passphrase, 0);
  BIO* bio = BIO_new(BIO_s_file());
  CFG_ASSERT(bio != nullptr);
  int status = BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_READ,
                        const_cast<char*>(filepath.c_str()));
  if (status <= 0) {
    memset(const_cast<char*>(final_passphrase.c_str()), 0,
           final_passphrase.size());
    CFG_INTERNAL_ERROR("Fail to read file %s for BIO read", filepath.c_str());
  }
  EVP_PKEY* key = nullptr;
  if (final_passphrase.size()) {
    // If passphrase is specified, then no need callback
    key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr,
                                  const_cast<char*>(final_passphrase.c_str()));
  } else {
    key = PEM_read_bio_PrivateKey(
        bio, nullptr, PASSPHRASE_CALLBACK,
        const_cast<char*>(
            CFG_print("Need passphrase to decrypt %s", filepath.c_str())
                .c_str()));
  }
  BIO_free_all(bio);
  if (key == nullptr) {
    memset(const_cast<char*>(final_passphrase.c_str()), 0,
           final_passphrase.size());
    CFG_INTERNAL_ERROR("Fail to read private EVP_PKEY from PEM BIO %s",
                       filepath.c_str());
  }
  bool supported_key = false;
  for (auto& c : CFGOpenSSL_KEY_INFO_DATABASE) {
    if (c.evp_pkey_id == EVP_PKEY_id(key)) {
      is_ec = c.evp_pkey_id == NID_X9_62_id_ecPublicKey;
      supported_key = true;
      break;
    }
  }
  CFG_ASSERT_MSG(supported_key, "Private EVP_PKEY type %d is not supported\n",
                 EVP_PKEY_id(key));
  return key;
}

void* CFGOpenSSL::read_public_key(const std::string& filepath,
                                  const std::string& passphrase, bool& is_ec,
                                  const bool allow_failure) {
  std::string final_passphrase = "";
  get_passphrase(final_passphrase, passphrase, 0);
  BIO* bio = BIO_new(BIO_s_file());
  CFG_ASSERT(bio != nullptr);
  int status = BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_READ,
                        const_cast<char*>(filepath.c_str()));
  if (status <= 0) {
    memset(const_cast<char*>(final_passphrase.c_str()), 0,
           final_passphrase.size());
    CFG_INTERNAL_ERROR("Fail to read file %s for BIO read", filepath.c_str());
  }
  EVP_PKEY* key = nullptr;
  if (final_passphrase.size()) {
    // If passphrase is specified, then no need callback
    key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr,
                              const_cast<char*>(final_passphrase.c_str()));
  } else {
    key = PEM_read_bio_PUBKEY(
        bio, nullptr, PASSPHRASE_CALLBACK,
        const_cast<char*>(
            CFG_print("Need passphrase to decrypt %s", filepath.c_str())
                .c_str()));
  }
  BIO_free_all(bio);
  if (key == nullptr) {
    memset(const_cast<char*>(final_passphrase.c_str()), 0,
           final_passphrase.size());
    if (allow_failure) {
      return nullptr;
    }
    CFG_INTERNAL_ERROR("Fail to read public EVP_PKEY from PEM BIO %s",
                       filepath.c_str());
  }
  bool supported_key = false;
  for (auto& c : CFGOpenSSL_KEY_INFO_DATABASE) {
    if (c.evp_pkey_id == EVP_PKEY_id(key)) {
      is_ec = c.evp_pkey_id == NID_X9_62_id_ecPublicKey;
      supported_key = true;
      break;
    }
  }
  CFG_ASSERT_MSG(supported_key, "Public EVP_PKEY type %d is not supported\n",
                 EVP_PKEY_id(key));
  return key;
}

void CFGOpenSSL::get_passphrase(std::string& final_passphrase,
                                const std::string& passphrase,
                                const uint32_t min_passphrase) {
  CFG_ASSERT(final_passphrase.size() == 0);
  if (passphrase.size()) {
    // Try it as a file first
    std::ifstream file(passphrase.c_str());
    if (file.is_open()) {
      CFG_POST_MSG("%s is treated as a file. Read first line as passphrase",
                   passphrase.c_str());
      std::getline(file, final_passphrase);
      file.close();
    } else {
      CFG_POST_MSG("Use the specified passphrase option as passphrase");
      final_passphrase = passphrase;
    }
    if (final_passphrase.size() < size_t(min_passphrase)) {
      CFG_POST_MSG(
          "Detected passphrase does not meet requirement of minimum %d "
          "characters. Will prompt passphrase if needed",
          min_passphrase);
      final_passphrase = "";
    }
  } else {
    // CFG_POST_WARNING(
    //     "Passphrase is not specified. Will prompt passphrase if needed");
  }
}

bool CFGOpenSSL::authenticate_message(
    const uint8_t* message, const size_t message_size, const uint8_t* signature,
    const size_t signature_size, const std::string& key_type,
    const uint8_t* pub_key, const uint32_t pub_key_size) {
  CFGCrypto_KEY key(key_type, pub_key, pub_key_size);
  return authenticate_message(message, message_size, signature, signature_size,
                              &key);
}

bool CFGOpenSSL::authenticate_message(const uint8_t* message,
                                      const size_t message_size,
                                      const uint8_t* signature,
                                      const size_t signature_size,
                                      const std::string& filepath,
                                      const std::string& passphrase) {
  CFGCrypto_KEY key(filepath, passphrase, false);
  return authenticate_message(message, message_size, signature, signature_size,
                              &key);
}

bool CFGOpenSSL::authenticate_message(const uint8_t* message,
                                      const size_t message_size,
                                      const uint8_t* signature,
                                      const size_t signature_size,
                                      CFGCrypto_KEY* key) {
  // maximum possible digest (SHA)
  uint8_t digest[64];
  // Use the nature of digest depends on key type
  size_t digest_size =
      key->get_digest(message, message_size, digest, sizeof(digest));
  return authenticate_digest(&digest[0], digest_size, signature, signature_size,
                             key);
}

bool CFGOpenSSL::authenticate_digest(
    const uint8_t* digest, const size_t digest_size, const uint8_t* signature,
    const size_t signature_size, const std::string& key_type,
    const uint8_t* pub_key, const uint32_t pub_key_size) {
  CFGCrypto_KEY key(key_type, pub_key, pub_key_size);
  return authenticate_digest(digest, digest_size, signature, signature_size,
                             &key);
}

bool CFGOpenSSL::authenticate_digest(const uint8_t* digest,
                                     const size_t digest_size,
                                     const uint8_t* signature,
                                     const size_t signature_size,
                                     const std::string& filepath,
                                     const std::string& passphrase) {
  CFGCrypto_KEY key(filepath, passphrase, false);
  return authenticate_digest(digest, digest_size, signature, signature_size,
                             &key);
}

bool CFGOpenSSL::authenticate_digest(const uint8_t* digest,
                                     const size_t digest_size,
                                     const uint8_t* signature,
                                     const size_t signature_size,
                                     CFGCrypto_KEY* key) {
  return key->verify_signature(digest, digest_size, signature, signature_size);
}

size_t CFGOpenSSL::sign_message(const uint8_t* message,
                                const size_t message_size, uint8_t* signature,
                                const size_t signature_size,
                                const std::string& filepath,
                                const std::string& passphrase) {
  CFGCrypto_KEY key(filepath, passphrase, true);
  return sign_message(message, message_size, signature, signature_size, &key);
}

size_t CFGOpenSSL::sign_message(const uint8_t* message,
                                const size_t message_size, uint8_t* signature,
                                const size_t signature_size,
                                CFGCrypto_KEY* key) {
  // maximum possible digest (SHA)
  uint8_t digest[64];
  // Use the nature of digest depends on key type
  size_t digest_size =
      key->get_digest(message, message_size, digest, sizeof(digest));
  return sign_digest(digest, digest_size, signature, signature_size, key);
}

size_t CFGOpenSSL::sign_digest(const uint8_t* digest, const size_t digest_size,
                               uint8_t* signature, const size_t signature_size,
                               const std::string& filepath,
                               const std::string& passphrase) {
  CFGCrypto_KEY key(filepath, passphrase, true);
  return sign_digest(digest, digest_size, signature, signature_size, &key);
}

size_t CFGOpenSSL::sign_digest(const uint8_t* digest, const size_t digest_size,
                               uint8_t* signature, const size_t signature_size,
                               CFGCrypto_KEY* key) {
  return key->sign(digest, digest_size, signature, signature_size);
}

void CFGOpenSSL::generate_random_data(uint8_t* data, const size_t data_size) {
  CFG_ASSERT(RAND_bytes(data, data_size) > 0);
}
uint32_t CFGOpenSSL::generate_random_u32() {
  uint32_t random = 0;
  generate_random_data((uint8_t*)(&random), sizeof(random));
  return random;
}
uint64_t CFGOpenSSL::generate_random_u64() {
  uint64_t random = 0;
  generate_random_data((uint8_t*)(&random), sizeof(random));
  return random;
}

void CFGOpenSSL::generate_iv(uint8_t* iv, bool gcm) {
  std::vector<uint8_t> data;
  std::string n = CFG_get_machine_name();
  uint64_t nt = CFG_get_unique_nano_time();
  CFG_append_u64(data, generate_random_u64());
  CFG_append_u64(data, nt);
  CFG_append_u32(data, CFG_get_volume_serial_number());
  data.insert(data.end(), n.begin(), n.end());
#if 0
  printf("Unique Time: 0x%016lX\n", nt);
  printf("Machine name: %s (%ld)\n", n.c_str(), n.size());
  printf("IV raw data:\n ");
  for (size_t i = 0; i < data.size(); i++) {
    printf(" %02X", data[i]);
    if ((i % 16) == 15) {
      printf("\n ");
    }
  }
  printf("\n");
#endif
  uint8_t sha512[64];
  sha_512(&data[0], data.size(), sha512);
  memset(iv, 0, 16);
  for (size_t i = 0, j = (gcm ? 4 : 0); i < sizeof(sha512); i++, j++) {
    if (j == 16) {
      j = (gcm ? 4 : 0);
    }
    iv[j] ^= sha512[i];
  }
  memset(&n[0], 0, n.size());
  memset(&data[0], 0, data.size());
  memset(sha512, 0, sizeof(sha512));
}
