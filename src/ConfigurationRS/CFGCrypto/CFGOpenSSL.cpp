#include "CFGCrypto_key.h"
#include "CFGOpenSSL_version_auto.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "openssl/x509.h"

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

static uint32_t get_der_compact_length(const uint8_t* data,
                                       uint32_t data_size) {
  CFG_ASSERT(data != nullptr && data_size > 0);
  uint32_t original_data_size = data_size;
  if (data[0] & 0x80) {
    return data_size + 1;
  } else {
    uint32_t index = 0;
    while (data_size) {
      if (data[index]) {
        if (data[index] & 0x80) {
          data_size++;
        }
        break;
      } else {
        data_size--;
        index++;
      }
    }
  }
  CFG_ASSERT(data_size);
  CFG_ASSERT(data_size <= original_data_size);
  return data_size;
}

#if CFGOpenSSL_SIMPLE_VERSION >= 30
#include "CFGOpenSSL_ver3.cpp"
#else
#include "CFGOpenSSL_ver1.cpp"
#endif
