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

/**
 * Determines the dimensions of the default browser scrollbar (irrespective of whether it's visible at the moment).
 * Some browsers overlay the scrollbar rather than putting it in-layout. In those cases, it's treated as 0-width/height.
 */
export function useScrollbarSize(): { width: number, height: number } {
  return React.useMemo(() => {
    const scroller = document.createElement('div');
    scroller.setAttribute('style', 'width: 100vw; height: 100vh; overflow: scroll; position: absolute; top: -100vh;');
    document.body.appendChild(scroller);
    const width = scroller.offsetWidth - scroller.clientWidth;
    const height = scroller.offsetHeight - scroller.clientHeight;
    document.body.removeChild(scroller);
    return { width, height };
  }, []);
}
