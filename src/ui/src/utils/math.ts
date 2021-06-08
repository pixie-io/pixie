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

/** Sign-agnostic minimum (closest value to zero) */
export function smallest(first: number, ...others: number[]): number {
  let found: number = null;
  for (const val of [first, ...others]) {
    if (found === null || Math.abs(val) < Math.abs(found)) {
      found = val;
    }
  }
  return found;
}

/** Clamps a number within the given range.  */
export function clamp(target: number, min: number, max: number): number {
  // In case the consumer flips the range arguments (such as by using negative numbers)
  const low = Math.min(min, max);
  const high = Math.max(min, max);
  return Math.max(low, Math.min(target, high));
}

/** If `lists` looks like an NxM matrix, this sums a target column. */
export function sumColumn(columnIndex: number, matrix: number[][]): number {
  return matrix.reduce((total, list) => total + list[columnIndex], 0);
}

/**
 * Scales each component of a sum in proportion such that they sum to a new value.
 * @param parts Array of numbers that sum up to some undesired value
 * @param newWhole What the numbers need to sum up to
 */
export function rescaleSum(parts: number[], newWhole: number): number[] {
  const oldSum = parts.reduce((a, c) => a + c, 0);
  const scale = newWhole / oldSum;
  return parts.map((ratio) => ratio * scale);
}
