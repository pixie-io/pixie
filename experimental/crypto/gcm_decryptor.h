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

#pragma once

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <string>

#include "src/common/base/base.h"

namespace px {

// Traditionally, a call to CRYPTO_library_init() is required before using BoringSSL functions
// However, it is only required if boringSSL is built with BORINGSSL_NO_STATIC_INITIALIZER.
//
// Confirmed with gdb that the internal do_library_init() is getting called automatically.
//
// From docs:
//
// Initialization
//
// OpenSSL has a number of different initialization functions for setting up error strings and
// loading algorithms, etc. All of these functions still exist in BoringSSL for convenience, but
// they do nothing and are not necessary.
//
// The one exception is CRYPTO_library_init. In BORINGSSL_NO_STATIC_INITIALIZER builds, it must
// be called to query CPU capabilities before the rest of the library. In the default
// configuration, this is done with a static initializer and is also unnecessary.

Status DecryptAESGCM(const EVP_CIPHER* cipher, const std::basic_string_view<uint8_t> key,
                     const std::basic_string_view<uint8_t> iv,
                     const std::basic_string_view<uint8_t> ciphertext,
                     const std::basic_string_view<uint8_t> aad,
                     const std::basic_string_view<uint8_t> tag,
                     std::basic_string<uint8_t>* plaintext);

}  // namespace px
