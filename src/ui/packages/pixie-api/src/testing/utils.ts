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

// This type finds the names of each method on a class, as well as the parameters of those methods.
export type Invocation<Class> = {
  [Key in keyof Class]: Class[Key] extends (...args: infer Params) => any
    ? (Params extends [] ? [Key] : any[]) // TODO(nick,PC-819): TS 4.x lets us change this line to `[Key, ...Params]`.
    : never;
}[keyof Class];
