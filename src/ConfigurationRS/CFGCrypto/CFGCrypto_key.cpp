#include "CFGOpenSSL_version_auto.h"

#if CFGOpenSSL_SIMPLE_VERSION >= 30
  #include "CFGCrypto_key_ver3.cpp"
#else
  #include "CFGCrypto_key_ver1.cpp"
#endif
