// Copyright (c) 2016-2019 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dile/dile_crypto.h>

#include "base/logging_pmlog.h"
#include "base/strings/string_util.h"
#include "components/os_crypt/os_crypt.h"
#include "crypto/random.h"

namespace {

// Salt for Symmetric key derivation.
const char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
const size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetic key derivation.
const size_t kEncryptionIterations = 1;

// Key size required for 128 bit AES.
const size_t kPasswordLength = 7;

}  // namespace

// static
std::string OSCrypt::unique_password_;

const char* OSCrypt::GetDileError(int error) {
  switch (error) {
    case DILE_NOT_OK:
      return "DILE_NOT_OK";
    case DILE_ERROR_WRONG_PARAM:
      return "DILE_ERROR_WRONG_PARAM";
    case DILE_ERROR_MEM_ALLOC:
      return "DILE_ERROR_MEM_ALLOC";
    case DILE_ERROR_WRONG_LENGTH:
      return "DILE_ERROR_WRONG_LENGTH";
    case DILE_ERROR_MVPD_ENC:
      return "DILE_ERROR_MVPD_ENC";
    case DILE_ERROR_MVPD_DEC:
      return "DILE_ERROR_MVPD_DEC";
    case DILE_ERROR_MVPD_B64ENC:
      return "DILE_ERROR_MVPD_B64ENC";
    case DILE_ERROR_MVPD_B64DEC:
      return "DILE_ERROR_MVPD_B64DEC";
    case DILE_ERROR_MVPD_STAT:
      return "DILE_ERROR_MVPD_STAT";
    default:
      // go through
      break;
  }
  return "DILE_ERROR_NOT_MATCHED";
}

// static
//
// Generates a newly allocated SymmetricKey object based
// a unique password for each device.
// Ownership of the key is passed to the caller.  Returns NULL key if a key
// generation error occurs.
std::unique_ptr<crypto::SymmetricKey> OSCrypt::GetUniqueEncryptionKey() {
  if (unique_password_.empty()) {
    unsigned char* read_key = nullptr;
    int read_key_size = 0;

    int dile_status = DILE_CRYPTO_ReadBrowserSecret(&read_key, &read_key_size);
    if (dile_status == DILE_OK) {
      VLOG(1) << "Reading browser crypt password succeeded";
      unique_password_ =
          std::string(reinterpret_cast<char*>(read_key), read_key_size);
    } else {
      VLOG(1) << "[" << GetDileError(dile_status)
              << "] Reading browser crypt password failed";
      crypto::RandBytes(base::WriteInto(&unique_password_, kPasswordLength + 1),
                        kPasswordLength);

      unsigned char* write_key = reinterpret_cast<unsigned char*>(
          const_cast<char*>(unique_password_.c_str()));

      // If DILE_CRYPTO_WriteBrowserSecret is failed, choose the default
      // |unique_password_|.
      dile_status = DILE_CRYPTO_WriteBrowserSecret(write_key, kPasswordLength);
      if (dile_status != DILE_OK) {
        VLOG(1) << "[" << GetDileError(dile_status)
                << "] Writing browser crypt password failed";
        unique_password_ = "peanuts";
      } else {
        VLOG(1) << "Writing browser crypt password succeeded";
      }
    }
    if (read_key)
      free(read_key);
  }

  // Create an encryption key from our password and salt.
  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, unique_password_, std::string(kSalt),
          kEncryptionIterations, kDerivedKeySizeInBits));
  DCHECK(encryption_key);

  return encryption_key;
}
