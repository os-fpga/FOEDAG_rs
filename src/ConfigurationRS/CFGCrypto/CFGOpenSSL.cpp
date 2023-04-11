#include "CFGOpenSSL_version_auto.h"

#if CFGOpenSSL_SIMPLE_VERSION >= 30
  #include "CFGOpenSSL_ver3.cpp"
#else
  #include "CFGOpenSSL_ver1.cpp"
#endif
