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

export function debounce<P extends any[] = any[]>(func: (...args: P) => void, wait: number): (...args: P) => void {
  let timeout;
  let context;
  let lastArgs;
  return function call(...args: P): void {
    // eslint-disable-next-line @typescript-eslint/no-this-alias
    context = this;
    lastArgs = args;
    const onTimeout = () => {
      func.apply(context, lastArgs);
      timeout = null;
    };
    const newTimeout = setTimeout(onTimeout, wait);
    if (timeout) {
      // Restart the timer if one already exists.
      clearTimeout(timeout);
    }
    timeout = newTimeout;
  };
}
