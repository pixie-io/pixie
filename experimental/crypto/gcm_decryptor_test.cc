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
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

extern "C" {
#include "aes-min/aes-min.h"
}

std::string HexString(const std::basic_string_view<uint8_t> s) {
  return px::BytesToString<px::bytes_format::Hex>(px::CreateStringView(s));
}

TEST(AES, key_recovery_basic) {
  std::basic_string<uint8_t> key = ::px::ConstString<uint8_t>("0123456789012345");
  std::basic_string<uint8_t> ks;
  ks.resize(AES128_KEY_SCHEDULE_SIZE);

  aes128_key_schedule(ks.data(), key.data());

  // Grab last 16 bytes (last key in key schedule).
  std::basic_string<uint8_t> recovered_key(ks.substr(ks.length() - 16, 16));
  ASSERT_EQ(recovered_key.size(), 16);
  aes128_recover_key(recovered_key.data());

  EXPECT_EQ(px::CreateStringView(key), px::CreateStringView(recovered_key));
}

TEST(AES, key_recovery_captured) {
  // Values captured from delve debugger.
  // Note that expected_ks is reported by the debugger as uint32 by delve.
  std::basic_string<uint8_t> key = {12, 10,  164, 169, 226, 47,  133, 244,
                                    19, 241, 96,  248, 1,   212, 194, 216};
  std::basic_string<uint32_t> expected_ks = {
      2846099980, 4102369250, 4167102739, 3636646913, 3586469701, 557842599,  3642814900,
      31598005,   12171128,   570013663,  4174988907, 4181418974, 488658311,  1020912216,
      3288376371, 1027327981, 1208465225, 1960727825, 2967400738, 2380609231, 3260706396,
      3062119245, 106613359,  2344615072, 577201868,  2497928577, 2461636590, 419851086,
      229837299,  2572178546, 199869340,  317696210,  3095058097, 556434115,  717439327,
      942467469,  3850217396, 3294045559, 4002702376, 3602419109, 3817589727, 668790440,
      3376974464, 535824165};

  // Generate key schedule from key.
  std::basic_string<uint8_t> ks;
  ks.resize(AES128_KEY_SCHEDULE_SIZE);
  aes128_key_schedule(ks.data(), key.data());

  EXPECT_EQ(px::CreateStringView(ks), px::CreateStringView(expected_ks));

  // Reconstruct master key from last round key.
  std::basic_string<uint8_t> recovered_key(ks.data() + ks.size() - 16, 16);
  ASSERT_EQ(recovered_key.size(), 16);
  aes128_recover_key(recovered_key.data());

  EXPECT_EQ(px::CreateStringView(key), px::CreateStringView(recovered_key));
}

TEST(DecryptTest, gcm_aes_128) {
  const EVP_CIPHER* cipher = EVP_aes_128_gcm();

  // Inputs.
  std::basic_string<uint8_t> key = ::px::ConstString<uint8_t>(
      "\xfe\xff\xe9\x92\x86\x65\x73\x1c\x6d\x6a\x8f\x94\x67\x30\x83\x08");
  std::basic_string<uint8_t> iv =
      ::px::ConstString<uint8_t>("\xca\xfe\xba\xbe\xfa\xce\xdb\xad\xde\xca\xf8\x88");
  std::basic_string<uint8_t> ciphertext = ::px::ConstString<uint8_t>(
      "\x42\x83\x1e\xc2\x21\x77\x74\x24\x4b\x72\x21\xb7\x84\xd0\xd4\x9c\xe3\xaa\x21\x2f\x2c\x02\xa4"
      "\xe0\x35\xc1\x7e\x23\x29\xac\xa1\x2e\x21\xd5\x14\xb2\x54\x66\x93\x1c\x7d\x8f\x6a\x5a\xac\x84"
      "\xaa\x05\x1b\xa3\x0b\x39\x6a\x0a\xac\x97\x3d\x58\xe0\x91");
  std::basic_string<uint8_t> ad = ::px::ConstString<uint8_t>(
      "\xfe\xed\xfa\xce\xde\xad\xbe\xef\xfe\xed\xfa\xce\xde\xad\xbe\xef\xab\xad\xda\xd2");
  std::basic_string<uint8_t> tag = ::px::ConstString<uint8_t>(
      "\x5b\xc9\x4f\xbc\x32\x21\xa5\xdb\x94\xfa\xe9\x5a\xe7\x12\x1a\x47");

  // Outputs.
  std::basic_string<uint8_t> plaintext;

  // Expectations.
  std::basic_string<uint8_t> expected_plaintext = ::px::ConstString<uint8_t>(
      "\xd9\x31\x32\x25\xf8\x84\x06\xe5\xa5\x59\x09\xc5\xaf\xf5\x26\x9a\x86\xa7\xa9\x53\x15\x34\xf7"
      "\xda\x2e\x4c\x30\x3d\x8a\x31\x8a\x72\x1c\x3c\x0c\x95\x95\x68\x09\x53\x2f\xcf\x0e\x24\x49\xa6"
      "\xb5\x25\xb1\x6a\xed\xf5\xaa\x0d\xe6\x57\xba\x63\x7b\x39");

  px::Status s = px::DecryptAESGCM(cipher, key, iv, ciphertext, ad, tag, &plaintext);
  ASSERT_OK(s);

  EXPECT_EQ(plaintext, expected_plaintext);
}

TEST(DecryptTest, gcm_aes_256) {
  const EVP_CIPHER* cipher = EVP_aes_256_gcm();

  // Inputs.
  std::basic_string<uint8_t> key = ::px::ConstString<uint8_t>(
      "\xfe\xff\xe9\x92\x86\x65\x73\x1c\x6d\x6a\x8f\x94\x67\x30\x83\x08\xfe\xff\xe9\x92\x86\x65\x73"
      "\x1c\x6d\x6a\x8f\x94\x67\x30\x83\x08");
  std::basic_string<uint8_t> iv =
      ::px::ConstString<uint8_t>("\xca\xfe\xba\xbe\xfa\xce\xdb\xad\xde\xca\xf8\x88");
  std::basic_string<uint8_t> ciphertext = ::px::ConstString<uint8_t>(
      "\x52\x2d\xc1\xf0\x99\x56\x7d\x07\xf4\x7f\x37\xa3\x2a\x84\x42\x7d\x64\x3a\x8c\xdc\xbf\xe5\xc0"
      "\xc9\x75\x98\xa2\xbd\x25\x55\xd1\xaa\x8c\xb0\x8e\x48\x59\x0d\xbb\x3d\xa7\xb0\x8b\x10\x56\x82"
      "\x88\x38\xc5\xf6\x1e\x63\x93\xba\x7a\x0a\xbc\xc9\xf6\x62\x89\x80\x15\xad");
  std::basic_string<uint8_t> ad;
  std::basic_string<uint8_t> tag = ::px::ConstString<uint8_t>(
      "\xb0\x94\xda\xc5\xd9\x34\x71\xbd\xec\x1a\x50\x22\x70\xe3\xcc\x6c");

  // Outputs.
  std::basic_string<uint8_t> plaintext;

  // Expectations.
  std::basic_string<uint8_t> expected_plaintext = ::px::ConstString<uint8_t>(
      "\xd9\x31\x32\x25\xf8\x84\x06\xe5\xa5\x59\x09\xc5\xaf\xf5\x26\x9a\x86\xa7\xa9\x53\x15\x34\xf7"
      "\xda\x2e\x4c\x30\x3d\x8a\x31\x8a\x72\x1c\x3c\x0c\x95\x95\x68\x09\x53\x2f\xcf\x0e\x24\x49\xa6"
      "\xb5\x25\xb1\x6a\xed\xf5\xaa\x0d\xe6\x57\xba\x63\x7b\x39\x1a\xaf\xd2\x55");

  px::Status s = px::DecryptAESGCM(cipher, key, iv, ciphertext, ad, tag, &plaintext);
  ASSERT_OK(s);

  EXPECT_EQ(plaintext, expected_plaintext);
}

// Real test data captured from https_client<->https_server.
TEST(DecryptTest, strace_capture_http1) {
  const EVP_CIPHER* cipher = EVP_aes_128_gcm();

  // Inputs.
  std::basic_string<uint8_t> ks = ::px::ConstString<uint8_t>(
      "\x91\xB9\xFA\x82\x3D\x77\x22\x81\x02\xF7\x95\x61\x17\xAA\x9B\x03");
  std::basic_string<uint8_t> iv =
      ::px::ConstString<uint8_t>("\x46\x72\x30\xF3\x87\xCE\xBE\xFC\xBF\x70\x21\x2A");
  std::basic_string<uint8_t> ciphertext = ::px::ConstString<uint8_t>(
      "\x82\x40\xfd\x20\x3e\x8a\x9c\x67\x89\x26\x67\x6e\x8b\x8b\xcd\x16\x31\x54\xd4\x9c\x73\x11\x97"
      "\x42\x7b\x0a\x5b\x86\x2a\xe7\xb5\xc9\x11\x27\x42\xb1\xad\x89\xc9\x58\x83\x6b\x0f\xc0\xfb\xc7"
      "\x7e\x16\x70\x86\xc0\x70\x98\x65\xa4\x1a\x28\x43\x58\xd2\xfd\xcf\x94\xda\x2c\x67\x72\x82\xf6"
      "\x67\x35\xda\xfd\x01\x89\x56\xf8\xa9\x9d\x6e\x92\x48\x43\x99\x43\xdd\xc8\x36\x26\xe1\x13\xa5"
      "\xed\xa8\x54\x43\x48\xeb\xf6\xbe\x82\xde\x34\x75\xfb\xf2\x27\x8d\x81\x17\xc5\x32\x8c\xfa\x28"
      "\xca\x23\x1c\xb9\x05\xca\xd8\x5e\x9b");
  std::basic_string<uint8_t> ad = ::px::ConstString<uint8_t>("\x17\x03\x03\x00\x8C");
  std::basic_string<uint8_t> tag = ::px::ConstString<uint8_t>(
      "\xDE\xF9\x01\xE5\x98\x8B\x94\x39\xF5\xE8\x29\x34\x3E\x4A\xE7\xB5");

  // Grab last 16 bytes (last key in key schedule).
  std::basic_string<uint8_t> key(ks.substr(ks.length() - 16, 16));
  ASSERT_EQ(key.size(), 16);
  aes128_recover_key(key.data());

  // Captured the master key by observing the connection being established via another probe.
  // Normally, we might not have access to the start of the connection, so we wouldn't be able to
  // directly probe this. That is why we are trying to regenerate the master key from the key
  // schedule.
  ASSERT_EQ(
      ::px::ConstStringView("\xF9\x8C\x63\x2B\xCE\x37\x34\xA2\xD9\xCD\x02\x2D\x5E\x6E\x1A\x43"),
      px::CreateStringView(key));

  // Outputs.
  std::basic_string<uint8_t> plaintext;

  // Expectations.
  std::basic_string<uint8_t> expected_plaintext = ::px::ConstString<uint8_t>(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Date: Thu, 21 Nov 2019 19:54:58 GMT\r\n"
      "Content-Length: 15\r\n"
      "\r\n"
      "{\"status\":\"ok\"}\x17");

  px::Status s = px::DecryptAESGCM(cipher, key, iv, ciphertext, ad, tag, &plaintext);
  ASSERT_OK(s);

  EXPECT_EQ(plaintext, expected_plaintext);
}

// Real test data captured from GRPC client<->server with TLS.
TEST(DecryptTest, strace_capture_grpc) {
  const EVP_CIPHER* cipher = EVP_aes_128_gcm();

  // Inputs.
  std::basic_string<uint8_t> ks = ::px::ConstString<uint8_t>(
      "\x19\x1A\x7E\xE8\xC6\xCE\x9A\xF4\x69\x90\xC9\x78\xD1\x7E\x33\xB6");

  std::basic_string<uint8_t> iv0 =
      ::px::ConstString<uint8_t>("\x4D\x39\x1D\xAF\xDD\x02\x01\x0A\x11\xE5\x90\xF5");
  std::basic_string<uint8_t> ciphertext0 = ::px::ConstString<uint8_t>(
      "\xb0\xd9\x7b\x96\xd2\xec\xf3\xb0\x05\xdf\xa5\x98\xbc\x7b\x84\x45\x4d\xb3\x38\x73\x09\x51\x11"
      "\xe8\x33\x35\xee\x23\x23\x5d\xf2");
  std::basic_string<uint8_t> ad0 = ::px::ConstString<uint8_t>("\x17\x03\x03\x00\x2f");
  std::basic_string<uint8_t> tag0 = ::px::ConstString<uint8_t>(
      "\x8c\x73\x49\xc9\x37\x1d\x7e\x1c\xd7\x8b\x57\x93\x9c\x5a\x28\x1e");

  std::basic_string<uint8_t> iv1 =
      ::px::ConstString<uint8_t>("\x4D\x39\x1D\xAF\xDD\x02\x01\x0A\x11\xE5\x90\xF4");
  std::basic_string<uint8_t> ciphertext1 = ::px::ConstString<uint8_t>(
      "\x2a\xb5\x4d\x9a\x54\x52\xb0\x93\x66\x9f\x46\x4a\x78\x23\x08\x3e\x26\xd6\x59\xec\xd1\x1e\xad"
      "\x40\xc9\xf9\xc7\xf3\x96\x2e\x76\x58\xf1\xcf\x8d\xcb\xec\x68\x76\x0f\x9a\x25\xd3\xdc\x5f\xb4"
      "\xad\x27\xdf\x82\xf3\x8c\x46\x90\xf0\x2e\x0b\xe6\x75\x45\x27\x6b\x85\xe1\xa7\xca\x03\xaa\x10"
      "\x9b\x09\x2e\x86\xd6\x2f\x27\xc4\xdb\xa3\x0d\x52\x4e\xca\x71");
  std::basic_string<uint8_t> ad1 = ::px::ConstString<uint8_t>("\x17\x03\x03\x00\x64");
  std::basic_string<uint8_t> tag1 = ::px::ConstString<uint8_t>(
      "\xe5\x42\x75\x5d\x55\xf2\x15\x10\xc8\xdb\xc6\xa3\x09\x78\x14\x54");

  // Grab last 16 bytes (last key in key schedule).
  std::basic_string<uint8_t> key(ks.substr(ks.length() - 16, 16));
  ASSERT_EQ(key.size(), 16);
  aes128_recover_key(key.data());

  // Outputs.
  std::basic_string<uint8_t> plaintext0;
  std::basic_string<uint8_t> plaintext1;

  ASSERT_OK(px::DecryptAESGCM(cipher, key, iv0, ciphertext0, ad0, tag0, &plaintext0));
  ASSERT_OK(px::DecryptAESGCM(cipher, key, iv1, ciphertext1, ad1, tag1, &plaintext1));

  EXPECT_TRUE(absl::StrContains(::px::CreateStringView(plaintext1), "Hello world"));
}
