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
 * A promise that additionally has two properties:
 * - cancelled: whether the `cancel` method has been invoked for this Promise.
 * - cancel: a function that will cause the function to reject with the string 'cancelled' when it would have otherwise
 *   resolved or rejected normally. Sets `cancelled` to true.
 */
export interface CancellablePromise<R extends unknown = void> extends Promise<R> {
  cancel: () => void;
  cancelled: boolean;
}

/**
 * Wraps the source Promise in a `cancel()`-able Promise.
 * Cancelling it causes it to reject immediately.
 * If you do not wish to do anything with a cancel-induced rejection, use the helper method `silentlyCatchCancellation`.
 * @param source
 */
export function makeCancellable<R extends unknown = void>(source: Promise<R>): CancellablePromise<R> {
  let cancelled = false;
  let cancel: () => void;

  const wrapped = new Promise((resolve, reject) => {
    cancel = () => {
      cancelled = true;
      reject(new Error('cancelled'));
    };

    // When the source Promise does try to resolve or reject, don't even attempt to resolve/reject with normal results.
    source.then(
      (result: R) => (cancelled ? reject(new Error('cancelled')) : resolve(result)),
      (error: any) => (cancelled ? reject(new Error('cancelled')) : reject(error)),
    );
  });

  Object.defineProperty(wrapped, 'cancel', {
    value: cancel,
    writable: false,
    configurable: false,
    enumerable: false,
  });

  Object.defineProperty(wrapped, 'cancelled', {
    get: () => cancelled,
  });

  return wrapped as CancellablePromise<R>;
}

/**
 * Convenience: if you don't care about rejections caused by cancelling a cancellable Promise, do this:
 * ```
 * const promise = makeCancellable(other);
 * promise.then(actOnNotCancelledResolution).catch(silentlyCatchCancellation).catch(handleRealRejection);
 * ```
 * And the error will be silenced ONLY if it came from `promise.cancel()`.
 */
// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export function silentlyCatchCancellation(e: any): void {
  if (typeof e !== 'object' || !e || e.message !== 'cancelled') {
    throw e;
  }
}
