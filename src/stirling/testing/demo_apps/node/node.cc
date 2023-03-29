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

// These code mimics class TLSWrap in
// https://github.com/nodejs/node/blob/master/src/crypto/crypto_tls.h.

class StreamResource {};

class StreamBase : public StreamResource {};

struct uv__io_s {
  int offset;
  int fd;
};
typedef struct uv__io_s uv__io_t;

struct uv_stream_s {
  uv__io_t io_watcher;
};
typedef struct uv_stream_s uv_stream_t;

class HandleWrap {
 public:
  HandleWrap() : offset_(0) {}
  int offset_;
};

class LibuvStreamWrap : public HandleWrap, public StreamBase {
 public:
  LibuvStreamWrap() : HandleWrap(), stream_(nullptr) {}
  uv_stream_t* const stream_;
};

class StreamListener {
 public:
  int offset_;
  StreamResource* stream_ = nullptr;
};

class TLSWrapParent {
 public:
  int offset_;
};

class TLSWrap : public TLSWrapParent, public StreamListener {};

int main() {
  // These are needed to avoid compiler omit dwarf info for unused types.
  TLSWrap tls_wrap;
  LibuvStreamWrap stream;
  uv_stream_t uv_stream;
}
