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

const encoding = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const encMap = new Map(encoding.split('').map((v, i) => [v, i]));
encMap.set('=', 0);

/** Throws if input isn't valid base64 (doesn't decode it, just checks validity). */
function validateBase64String(input: string, allowMiddlePadding = false): void {
  if (!input || typeof input !== 'string') {
    throw new Error('Incorrect base64 encoding: must be a string with non-zero length');
  }
  if (!/^[A-Za-z0-9+/=]*$/.test(input)) {
    throw new Error('Incorrect base64 encoding: may only contain A-Z, a-z, 0-9, +, /, and = characters.');
  }
  if (input.length % 4 !== 0) {
    throw new Error('Incorrect base64 encoding: length not divisible by 4.');
  }

  if (allowMiddlePadding) {
    // Split when discovering padding strings, and validate them separately.
    // Not every base64 string has padding, so this won't catch every problem but it helps.
    const firstPad = input.match(/=+/);
    if (firstPad != null && firstPad.index + firstPad[0].length < input.length) {
      const len = firstPad[0].length;
      validateBase64String(input.substring(0, firstPad.index + len), allowMiddlePadding);
      validateBase64String(input.substring(firstPad.index + len), allowMiddlePadding);
    }
  } else {
    const where = input.indexOf('=');
    const padding = where >= 0 ? input.substring(input.indexOf('=')) : '';
    if (padding.length && !/^=*$/.test(padding)) {
      throw new Error('Incorrect base64 encoding: padding characters may only appear at the end.');
    }
    if (padding.length > 3) {
      throw new Error('Incorrect base64 encoding: too many padding characters (max 3).');
    }
  }
}

/**
 * Decode a base64-encoded string directly to binary, in the form of a Uint8Array.
 * JavaScript already provides `atob`, but that goes to a text encoding.
 * For data that isn't text, this usually results in incorrect output.
 *
 * Base64 maps each possible 6-bit value to an ASCII character:
 *   A = 0 (0b000000), Z = 25, (0b011001), a = 26, z = 51, `0` = 52, `9` = 61, `+` = 62, `/` = 63.
 * The length of the string must be divisible by 4 (as 4*6 bits = 24 bits = clean 3 bytes).
 * If it isn't, `=` characters are added to the end (treated as 0b000000) to pad it out.
 * Conversion is done in groups of 3 bytes / 4 characters.
 *
 * @param input Base64-encoded string, encoding binary data
 * @param allowMiddlePadding If set, assume input is a bunch of base64 strings concatenated together.
 *        Padding may occur in the middle; it's assumed to be safe in this setup even if it's not.
 * @returns Uint8Array with the Base64 decoded directly to bytes, no further processing.
 */
export function atou(input: string, allowMiddlePadding = false): Uint8Array {
  validateBase64String(input, allowMiddlePadding);

  const numBytes = input.length * 3 / 4;
  const arr = new Uint8Array(numBytes);

  // Each character represents 6 bits; 4 characters * 6 bits = 24 bits = 3 bytes.
  // Thus, operate in groups of 4 characters.
  const chars = input.split('').map((c) => encMap.get(c));
  for (let c = 0; c < chars.length; c += 4) {
    // Assemble a 24-bit value, then extract the assembled bytes
    const word = 0
      + ((chars[c + 0] & 0x3F) << 18)
      + ((chars[c + 1] & 0x3F) << 12)
      + ((chars[c + 2] & 0x3F) << 6)
      + ((chars[c + 3] & 0x3F) << 0);
    const align = c * 3 / 4;
    arr[align + 0] = (word & 0xFF0000) >> 16;
    arr[align + 1] = (word & 0x00FF00) >> 8;
    arr[align + 2] = (word & 0x0000FF) >> 0;
  }

  return arr;
}
