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

import * as React from 'react';

import { makeCancellable } from 'app/utils/cancellable-promise';

/**
 * useEffect, except it runs the effect one render cycle after it's actually triggered.
 * Useful if a dependency of the effect takes one cycle to trigger and another to know properties (like DOM dimensions).
 * The delayed effect is cancelled if deps change or if the component unmounts before the effect would run.
 */
export const useDelayedEffect: typeof React.useEffect = (effect, deps) => {
  React.useEffect(() => {
    const delayedUpdater = makeCancellable(new Promise((resolve) => setTimeout(resolve)));
    delayedUpdater.then(effect);
    return () => delayedUpdater.cancel();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);
};
