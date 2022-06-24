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
export interface CancellablePromise<R = void> extends Promise<R> {
  cancel: () => void;
  cancelled: boolean;
}

/**
 * Wraps the source Promise in a `cancel()`-able Promise.
 * Cancelling it causes it to reject immediately if rejectOnCancel is true; otherwise the promise simply never settles.
 * Cancelling a promise that already settled still sets `cancelled`
 * @param source Promise to make cancellable
 * @param rejectOnCancel If set, calling `cancel()` immediately rejects the wrapped promise instead of cutting it off.
 * @param onCancel If set, calling `cancel()` will cancel the promise, call onCancel, and then do rejectOnCancel logic.
 */
export function makeCancellable<R = void>(
  source: Promise<R>,
  rejectOnCancel = false,
  onCancel?: (() => void),
): CancellablePromise<R> {
  let cancelled = false;
  let cancel: () => void;

  const wrapped = new Promise((resolve, reject) => {
    cancel = () => {
      cancelled = true;
      onCancel?.();
      if (rejectOnCancel) reject(new Error('cancelled'));
    };

    // When the source Promise does try to resolve or reject, don't even attempt to resolve/reject with normal results.
    source.then(
      (result: R) => (cancelled ? (rejectOnCancel && reject(new Error('cancelled'))) : resolve(result)),
      (error: any) => (cancelled ? (rejectOnCancel && reject(new Error('cancelled'))) : reject(error)),
    );
  });

  Object.defineProperty(wrapped, 'cancel', {
    value: cancel,
    writable: false,
    configurable: false,
    enumerable: false,
  }).then();

  Object.defineProperty(wrapped, 'cancelled', {
    get: () => cancelled,
  }).then();

  return wrapped as CancellablePromise<R>;
}
