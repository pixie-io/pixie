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

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

// The amount of bytes in a single slice of data.
// This value was not chosen according to some constant in the grpc-c library.
// Largest I saw was 1293.
#define GRPC_C_SLICE_SIZE (16380)

// This needs to not be lower than 8 (which is the maximum amount of inlined
// slices in a grpc_slice_buffer). The real maximum size isn't known - it can
// probably be larger than 8. Until now I have not seen a size larger than 2 used,
// so 8 is more than enough.
#define SIZE_OF_DATA_SLICE_ARRAY (8)

#define GRPC_C_DEFAULT_MAP_SIZE (10240)

#define MAXIMUM_AMOUNT_OF_ITEMS_IN_METADATA (30)
#define MAXIMUM_LENGTH_OF_KEY_IN_METADATA (44)
#define MAXIMUM_LENGTH_OF_VALUE_IN_METADATA (100)

enum grpc_c_version_t {
  GRPC_C_VERSION_UNSUPPORTED = 0,
  GRPC_C_V1_19_0,
  GRPC_C_V1_24_1,
  GRPC_C_V1_33_2,
  GRPC_C_V1_41_1,
  GRPC_C_VERSION_LAST
};

struct grpc_c_data_slice_t {
  uint32_t length;
  char bytes[GRPC_C_SLICE_SIZE];
};
// This must be aligned to 8-bytes.
// Because of this, the length of the bytes array
// must be (length % 8) == 4 to accommodate for the uint32_t.
#ifdef __cplusplus
static_assert((sizeof(struct grpc_c_data_slice_t) % 8) == 0,
              "gRPC-C data slice is not aligned to 8-bytes.");
#endif

struct grpc_c_metadata_item_t {
  uint32_t key_size;
  char key[MAXIMUM_LENGTH_OF_KEY_IN_METADATA];
  uint32_t value_size;
  char value[MAXIMUM_LENGTH_OF_VALUE_IN_METADATA];
};

struct grpc_c_metadata_t {
  uint64_t count;
  struct grpc_c_metadata_item_t items[MAXIMUM_AMOUNT_OF_ITEMS_IN_METADATA];
};

struct grpc_c_header_event_data_t {
  struct conn_id_t conn_id;
  uint32_t stream_id;
  uint64_t timestamp;
  enum traffic_direction_t direction;
  struct grpc_c_metadata_item_t header;
};

struct grpc_c_event_data_t {
  struct conn_id_t conn_id;
  uint32_t stream_id;
  uint64_t timestamp;
  enum traffic_direction_t direction;
  uint64_t position_in_stream;
  struct grpc_c_data_slice_t slice;
};

struct grpc_c_stream_closed_data {
  struct conn_id_t conn_id;
  uint32_t stream_id;
  uint64_t timestamp;
  enum traffic_direction_t direction;
  uint32_t read_closed;
  uint32_t write_closed;
};
