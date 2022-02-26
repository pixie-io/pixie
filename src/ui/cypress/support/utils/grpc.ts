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

import { Interception, RouteHandler } from 'cypress/types/net-stubbing';

import { ExecuteScriptRequest, ExecuteScriptResponse } from 'app/types/generated/vizierapi_pb';

const SERVICE_ROOT = 'px.api.vizierpb.VizierService';

// Directly convert a base64 string into a Uint8Array (no atob, which tries to mess with UTF-8 along the way)
// Base64 works like this:
// - Every character represents six bits.
// - `A` = 0, `Z` = 25 (0b011001), `a` = 26, `z` = 51, `0` = 52, ..., `9` = 61, `+` = 62, `/` = 63.
// - `=` = 0 as well; it's padding to make the number of characters divisible by 4.
// - Every set of 4 characters (6*4 = 24 bits) represents 3 bytes.
// - Thus, numBytes = numChars * 3/4.
export function atou(input: string): Uint8Array {
  const encoding = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'; // = is special
  const encMap = encoding.split('').reduce((a, c, i) => ({ ...a, [c]: i }), { '=': 0 });

  // Note: no verification being done here. User beware.
  const numBytes = input.length * 3 / 4;
  const arr = new Uint8Array(numBytes);

  // Each character represents 6 bits; 4 characters * 6 bits = 24 bits = 3 bytes.
  // Thus, operate in groups of 4 characters.
  const chars = input.split('').map((c) => encMap[c]);
  for (let c = 0; c < chars.length; c += 4) {
    // 4 characters representing 6 bits each, to binary, combine -> 24 bits (3 bytes)
    const bitStr = chars.slice(c, c + 4).map((cc) => cc.toString(2).padStart(6, '0')).join('');
    const align = c * 3 / 4;
    arr[align + 0] = parseInt(bitStr.substring(0, 8), 2);
    arr[align + 1] = parseInt(bitStr.substring(8, 16), 2);
    arr[align + 2] = parseInt(bitStr.substring(16, 24), 2);
  }

  return arr;
}

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
  const allBytes = atou(input);
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

export interface InterceptExecuteScriptOptions {
  /** If provided, becomes the second argument to cy.intercept. Use for stubbing. */
  routeHandler?: RouteHandler;
}
export function interceptExecuteScript(opts: InterceptExecuteScriptOptions = {}): Cypress.Chainable<null> {
  const url = `${Cypress.config().baseUrl}/${SERVICE_ROOT}/ExecuteScript`;

  // TODO: Use opts to filter what gets intercepted and what doesn't.
  return cy.intercept(url, opts.routeHandler ?? undefined);
}

export interface DeserializedIntercept extends Interception {
  reqJson: ExecuteScriptRequest.AsObject;
  resJson: ExecuteScriptResponse.AsObject[];
}

export function waitExecuteScript(alias: string): Cypress.Chainable<DeserializedIntercept> {
  return cy.wait(alias).then(({ request, response, ...rest }: Interception) => {
    const reqJson = deserializeExecuteScriptRequest(request.body).toObject();
    const resJson = deserializeExecuteScriptResponse(response.body).map((m) => m.toObject());
    return {
      ...rest,
      request,
      response,
      reqJson,
      resJson,
    };
  });
}
