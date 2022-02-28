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

import { atou } from 'app/testing/utils/base64';
import { ExecuteScriptRequest, ExecuteScriptResponse } from 'app/types/generated/vizierapi_pb';

export function deserializeExecuteScriptRequest(input: string): ExecuteScriptRequest {
  // Skips the first five bytes: one for compression, four for message length
  // This proto is transmitted as-is and alone; no trailers or the like to deal with.
  return ExecuteScriptRequest.deserializeBinary(atou(input).subarray(5));
}

export function deserializeExecuteScriptResponse(input: string): ExecuteScriptResponse[] {
  // content-type: "application/grpc-web-text+proto"
  // Each proto is a single byte for compression status, then four for message length, then the binary message.
  //  https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-WEB.md#protocol-differences-vs-grpc-over-http2
  //  https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md
  //  https://developers.google.com/protocol-buffers/docs/encoding
  // TODO: Sometimes there's an extra null byte or two between two messages; no idea why or how to replicate it.
  // TODO: The trailer starts with byte 128; figure out what it is and how to replicate that (mock responses).
  const allBytes = atou(input, true);
  let offset = 0;
  const messages = [];
  while (offset < allBytes.length) {
    const compression = allBytes[offset];
    if (compression === 1) {
      throw new Error(`Compression enabled at offset 0x${offset.toString(16)}, parsing likely wrong`);
    } else if (compression === 128) {
      // We're in the message trailer now, no more messages to parse.
      break;
    } else if (compression !== 0) {
      throw new Error(`Nonsense compression byte 0x${compression.toString(16)} at offset 0x${offset.toString(16)}!`);
    }

    const length = allBytes.subarray(offset + 1, offset + 5).reduce((a, c, i) => (
      a + (c << ((3 - i) * 8))
    ), 0);

    // TODO: These bytes are always first in ExecuteScriptResponse; magic numbers bad.
    //  Should instead learn why these extra zeroes appear, predict them, and skip that way.
    if (allBytes[offset + 5] !== 18 || allBytes[offset + 6] !== 36) {
      offset++;
      continue;
    }

    const bytes = allBytes.subarray(offset + 5, offset + 5 + length);
    messages.push(ExecuteScriptResponse.deserializeBinary(bytes));
    offset = offset + 5 + length;
  }

  return messages;
}
