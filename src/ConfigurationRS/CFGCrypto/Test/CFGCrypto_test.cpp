#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGCrypto_key.h"
#include "CFGOpenSSL.h"

void test_sha() {
  CFG_POST_MSG("SHA Test");
  std::vector<uint8_t> data;
  for (int i = 0; i < 16; i++) {
    data.push_back((uint8_t)(i));
  }

  // Result
  std::vector<uint8_t> sha256(32);
  const char* expected_sha256 =
      "\xBE\x45\xCB\x26\x05\xBF\x36\xBE\xBD\xE6\x84\x84\x1A\x28\xF0\xFD\x43\xC6"
      "\x98\x50\xA3\xDC\xE5\xFE\xDB\xA6\x99\x28\xEE\x3A\x89\x91";
  std::vector<uint8_t> sha384(48);
  const char* expected_sha384 =
      "\xC8\x1D\xF9\x8D\x9E\x6D\xE9\xB8\x58\xA1\xE6\xEB\xA0\xF1\xA3\xA3\x99\xD9"
      "\x8C\x44\x1E\x67\xE1\x06\x26\x01\x80\x64\x85\xBB\x89\x12\x5E\xFD\x54\xCC"
      "\x78\xDF\x5F\xBC\xEA\xBC\x93\xCD\x7C\x7B\xA1\x3B";
  std::vector<uint8_t> sha512(64);
  const char* expected_sha512 =
      "\xDA\xA2\x95\xBE\xED\x4E\x2E\xE9\x4C\x24\x01\x5B\x56\xAF\x62\x6B\x4F\x21"
      "\xEF\x9F\x44\xF2\xB3\xD4\x0F\xC4\x1C\x90\x90\x0A\x6B\xF1\xB4\x86\x7C\x43"
      "\xC5\x7C\xDA\x54\xD1\xB6\xFD\x48\x69\xB3\xF2\x3C\xED\x5E\x0B\xA3\xC0\x5D"
      "\x0B\x16\x80\xDF\x4E\xC7\xD0\x76\x24\x03";

  // 256
  CFGOpenSSL::sha_256(&data[0], data.size(), &sha256[0]);
  CFG_ASSERT(memcmp(&sha256[0], expected_sha256, sha256.size()) == 0);

  // 384
  CFGOpenSSL::sha_384(&data[0], data.size(), &sha384[0]);
  CFG_ASSERT(memcmp(&sha384[0], expected_sha384, sha384.size()) == 0);

  // 512
  CFGOpenSSL::sha_512(&data[0], data.size(), &sha512[0]);
  CFG_ASSERT(memcmp(&sha512[0], expected_sha512, sha512.size()) == 0);
}

void test_encryption() {
  CFG_POST_MSG("Encryption Test");
  const size_t data_size = 34;
  std::vector<uint8_t> data;
  for (size_t i = 0; i < data_size; i++) {
    data.push_back((uint8_t)(i));
  }
  std::vector<uint8_t> key(16, 0);
  std::vector<uint8_t> iv(16, 0);
  std::vector<uint8_t> cipher(data_size);
  std::vector<uint8_t> expected_cipher = {
      0x48, 0x45, 0x8A, 0xC5, 0xE1, 0x0A, 0x8E, 0x17, 0x67, 0x29, 0x0B, 0xAC,
      0xB4, 0x43, 0xCE, 0xF5, 0x63, 0x7D, 0xBD, 0x76, 0x13, 0x71, 0x7A, 0x3F,
      0xAE, 0x01, 0x92, 0xD0, 0x44, 0xA5, 0xDA, 0x39, 0x85, 0xA9};
  std::vector<uint8_t> plain(data_size);
  key[1] = 0xAA;
  iv[14] = 0xCC;
  CFGOpenSSL::ctr_encrypt(&data[0], &cipher[0], data.size(), &key[0],
                          key.size(), &iv[0], iv.size());
  CFG_ASSERT(cipher.size() == expected_cipher.size());
  for (size_t i = 0; i < data_size; i++) {
    CFG_ASSERT(cipher[i] == expected_cipher[i]);
  }
  CFGOpenSSL::ctr_decrypt(&cipher[0], &plain[0], cipher.size(), &key[0],
                          key.size(), &iv[0], iv.size());
  for (size_t i = 0; i < data_size; i++) {
    CFG_ASSERT(plain[i] == (uint8_t)(i));
  }
}

void test_signing() {
  CFG_POST_MSG("Signing Test");
  std::vector<std::string> keys = {"rsa2048", "prime256v1"};
  uint8_t signature[1024];
  std::vector<uint8_t> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  for (int i = 0; i < 20; i++) {
    size_t signature_size = 256;
    for (auto key : keys) {
      printf("  Testing %s\n", key.c_str());
      std::string passphrase =
          (((i % 10) != 0)
               ? ""
               : CFG_print("This is testing %s number #%d", key.c_str(), i));
      std::vector<uint8_t> pubkey;
      CFGOpenSSL::gen_private_pem(key, "private.pem", passphrase,
                                  passphrase.empty());
      CFGOpenSSL::gen_public_pem("private.pem", "public.pem", passphrase);
      CFGCrypto_KEY sign_key("private.pem", passphrase, true);
      sign_key.get_public_key(pubkey, 0);
      CFG_ASSERT(signature_size ==
                 CFGOpenSSL::sign_message(&data[0], data.size(), signature,
                                          sizeof(signature), &sign_key));
      CFG_ASSERT(CFGOpenSSL::authenticate_message(
          &data[0], data.size(), signature, signature_size, key, &pubkey[0],
          (uint32_t)(pubkey.size())));
      CFG_ASSERT(CFGOpenSSL::authenticate_message(
          &data[0], data.size(), signature, signature_size, "public.pem", ""));
      CFG_ASSERT(CFGOpenSSL::authenticate_message(&data[0], data.size(),
                                                  signature, signature_size,
                                                  "private.pem", passphrase));
      signature_size = 64;
    }
  }
}

int main(int argc, const char** argv) {
  CFG_POST_MSG("This is CFGOpenSSL unit test");
  test_sha();
  test_encryption();
  test_signing();
  return 0;
}
