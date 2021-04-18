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

/**
 * Use in the default of a switch/case block to prove that all values
 * have been considered. Fails to compile if any were forgotten at compile
 * time, and throws if an unexpected value is encountered at runtime.
 *
 * @param val The value that is being switch/cased
 */
export function checkExhaustive(val: never): never {
  throw new Error(`Unexpected value: ${JSON.stringify(val)}`);
}
