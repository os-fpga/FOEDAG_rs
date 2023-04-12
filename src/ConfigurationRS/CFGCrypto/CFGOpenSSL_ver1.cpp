static EC_GROUP* generate_ec_group(int nid) {
  EC_GROUP* group = EC_GROUP_new_by_curve_name(nid);
  CFG_ASSERT(group != nullptr);
  EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);
  EC_GROUP_set_point_conversion_form(group, POINT_CONVERSION_UNCOMPRESSED);
  return group;
}

static EC_KEY* generate_ec_key(EC_GROUP* group) {
  CFG_ASSERT(group != nullptr);
  EC_KEY* key = EC_KEY_new();
  CFG_ASSERT(key != nullptr);
  CFG_ASSERT(EC_KEY_set_group(key, group) != 0);
  CFG_ASSERT(EC_KEY_generate_key(key) != 0);
  return key;
}

static RSA* generate_rsa_key(uint32_t byte_size) {
  // Minimum 512 bits
  // Multiple of 256 bits
  CFG_ASSERT((byte_size * 8) >= 512);
  CFG_ASSERT(((byte_size * 8) % 256) == 0);
  BIGNUM* bn = BN_new();
  int status = BN_set_word(bn, RSA_F4);
  if (status <= 0) {
    BN_free(bn);
    CFG_INTERNAL_ERROR("Fail to BN_set_word");
  }

  RSA* rsa = RSA_new();
  status = RSA_generate_key_ex(rsa, byte_size * 8, bn, nullptr);
  if (status <= 0) {
    BN_free(bn);
    RSA_free(rsa);
    CFG_INTERNAL_ERROR("Fail to RSA_generate_key_ex");
  }
  return rsa;
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
  memset(internal_iv, 0, sizeof(internal_iv));
}

void CFGOpenSSL::gen_private_pem(const std::string& key_type,
                                 const std::string& filepath,
                                 const std::string& passphrase,
                                 const bool skip_passphrase) {
  init_openssl();
  EC_GROUP* group = nullptr;
  EC_KEY* ec_key = nullptr;
  RSA* rsa_key = nullptr;
  const CFGOpenSSL_KEY_INFO* key_info = get_key_info(key_type);
  if (key_info->evp_pkey_id == NID_X9_62_id_ecPublicKey) {
    group = generate_ec_group(key_info->nid);
    ec_key = generate_ec_key(group);
  } else {
    rsa_key = generate_rsa_key(key_info->size);
  }
  BIO* bio = BIO_new(BIO_s_file());
  CFG_ASSERT(bio != nullptr);
  int status = BIO_ctrl(bio, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_WRITE,
                        const_cast<char*>(filepath.c_str()));
  CFG_ASSERT_MSG(status > 0, "Fail to open file %s for BIO write",
                 filepath.c_str());
  if (skip_passphrase) {
    if (key_info->evp_pkey_id == NID_X9_62_id_ecPublicKey) {
      CFG_ASSERT(PEM_write_bio_ECPKParameters(bio, group) > 0);
      CFG_ASSERT(PEM_write_bio_ECPrivateKey(bio, ec_key, nullptr, nullptr, 0,
                                            nullptr, nullptr) > 0);
      EC_KEY_free(ec_key);
    } else {
      CFG_ASSERT(PEM_write_bio_RSAPrivateKey(bio, rsa_key, nullptr, nullptr, 0,
                                             nullptr, nullptr) > 0);
      RSA_free(rsa_key);
    }
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
    EVP_PKEY* evp_key = nullptr;
    PKCS8_PRIV_KEY_INFO* p8inf = nullptr;
    X509_ALGOR* pbe = nullptr;
    X509_SIG* p8 = nullptr;
    evp_key = EVP_PKEY_new();
    if (evp_key == nullptr) {
      memory_clean(final_passphrase, evp_key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to create EVP Key");
    }
    if (key_info->evp_pkey_id == NID_X9_62_id_ecPublicKey) {
      status = EVP_PKEY_assign(evp_key, NID_X9_62_id_ecPublicKey, ec_key);
    } else {
      status = EVP_PKEY_assign(evp_key, NID_rsaEncryption, rsa_key);
    }
    if (status <= 0) {
      memory_clean(final_passphrase, evp_key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to assign EC Key to EVP Key");
    }
    // Convert EVP Key to PKCS8
    p8inf = EVP_PKEY2PKCS8(evp_key);
    if (p8inf == nullptr) {
      memory_clean(final_passphrase, evp_key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to convert EVP Key to PKCS8");
    }
    // Create PBE algorithm from ass256-cbc EVP_CIPHER and pbe_nid
    pbe = PKCS5_pbe2_set_iv(EVP_aes_256_cbc(), 5000000, nullptr, 0, nullptr,
                            NID_hmacWithSHA512);
    if (pbe == nullptr) {
      memory_clean(final_passphrase, evp_key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to create PBE X509_ALGOR");
    }
    // Encrypt
    p8 = PKCS8_set0_pbe(final_passphrase.c_str(), int(final_passphrase.size()),
                        p8inf, pbe);
    if (p8 == nullptr) {
      memory_clean(final_passphrase, evp_key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to create PKCS8 X509_SIG");
    }
    // Write to BIO
    status = PEM_write_bio_PKCS8(bio, p8);
    if (status <= 0) {
      memory_clean(final_passphrase, evp_key, p8inf, p8);
      CFG_INTERNAL_ERROR("Fail to write X509_SIG to PEM BIO");
    }
    // Free
    memory_clean(final_passphrase, evp_key, p8inf, p8);
  }
  if (group != nullptr) {
    EC_GROUP_free(group);
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
    EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key);
    status = PEM_write_bio_EC_PUBKEY(bio, ec_key);
    CFG_ASSERT_MSG(status > 0, "Fail to write EC Public Key to PEM BIO");
  } else {
    CFG_POST_MSG("Detected private RSA Key");
    RSA* rsa_key = EVP_PKEY_get0_RSA(key);
    status = PEM_write_bio_RSAPublicKey(bio, rsa_key);
    CFG_ASSERT_MSG(status > 0, "Fail to write RSA Public Key to PEM BIO");
  }
  EVP_PKEY_free(key);
  BIO_free_all(bio);
}
