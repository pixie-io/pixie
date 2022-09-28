/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "experimental/crypto/gcm_decryptor.h"

#include <cstring>

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#define PL_RETURN_IF_FALSE(x, err) \
  if (!x) {                        \
    return err;                    \
  }

namespace px {

Status DecryptAESGCM(const EVP_CIPHER* cipher, const std::basic_string_view<uint8_t> key,
                     const std::basic_string_view<uint8_t> iv,
                     const std::basic_string_view<uint8_t> ciphertext,
                     const std::basic_string_view<uint8_t> ad,
                     const std::basic_string_view<uint8_t> tag,
                     std::basic_string<uint8_t>* plaintext) {
  // Status of SSL library calls.
  int s;

  // Output length from SSL library calls (initialize to 0 just out of paranoia).
  int outl = 0;

  // Total length of the decrypted data.
  int len = 0;

  // TODO(oazizi): This value was copied from a sample code. Determine an appropriate value and
  // clean-up.
  plaintext->resize(4096);

  if (EVP_CIPHER_key_length(cipher) != key.size()) {
    return error::Internal("Key length doesn't match");
  }

  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);

  s = EVP_DecryptInit_ex(&ctx, cipher, nullptr, nullptr, nullptr);
  PL_RETURN_IF_FALSE(s, error::Internal("EncryptInit failed"));

  s = EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr);
  PL_RETURN_IF_FALSE(s, error::Internal("IV length set failed"));

  s = EVP_DecryptInit_ex(&ctx, nullptr, nullptr, key.data(), iv.data());
  PL_RETURN_IF_FALSE(s, error::Internal("Key/IV set failed"));

  s = EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                          const_cast<void*>(reinterpret_cast<const void*>(tag.data())));
  PL_RETURN_IF_FALSE(s, error::Internal("Set tag failed"));

  if (!ad.empty()) {
    s = EVP_DecryptUpdate(&ctx, nullptr, &outl, ad.data(), ad.size());
    PL_RETURN_IF_FALSE(s, error::Internal("AAD set failed"));
  }

  EVP_CIPHER_CTX_set_padding(&ctx, 0);

  s = EVP_DecryptUpdate(&ctx, plaintext->data(), &outl, ciphertext.data(), ciphertext.size());
  PL_RETURN_IF_FALSE(s, error::Internal("Decrypt failed"));
  len += outl;

  s = EVP_DecryptFinal_ex(&ctx, plaintext->data() + plaintext->size(), &outl);
  PL_RETURN_IF_FALSE(s, error::Internal("DecryptFinal failed"));
  len += outl;

  plaintext->resize(len);

  EVP_CIPHER_CTX_cleanup(&ctx);

  return Status::OK();
}

}  // namespace px
