#include "CFGCrypto_key.h"
#include "CFGOpenSSL_version_auto.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "openssl/x509.h"

#define MIN_PASSPHRASE_SIZE (13)
const std::vector<CFGOpenSSL_KEY_INFO> CFGOpenSSL_KEY_INFO_DATABASE = {
    CFGOpenSSL_KEY_INFO(NID_X9_62_prime256v1, NID_X9_62_id_ecPublicKey,
                        "prime256v1", 32, NID_sha256, 32, 1),
    CFGOpenSSL_KEY_INFO(NID_rsa, NID_rsaEncryption, "rsa2048", 256, NID_sha256,
                        32, 2)};

#if CFGOpenSSL_SIMPLE_VERSION >= 30
#include "CFGOpenSSL_ver3.cpp"
#else
#include "CFGOpenSSL_ver1.cpp"
#endif
