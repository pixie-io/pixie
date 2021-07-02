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
import AutoSizer from 'react-virtualized-auto-sizer';

export const AutoSizerContext = React.createContext<{ width: number, height: number }>({ width: 0, height: 0 });

/** Wraps component in an <AutoSizer>, and provides the width/height properties that creates in AutoSizerContext. */
export function withAutoSizerContext<T>(Component: React.ComponentType<T>): React.ComponentType<T> {
  const Wrapped: React.FC<T> = (props) => (
    <AutoSizer>
      {({ width, height }) => (
        <AutoSizerContext.Provider value={{ width: Math.max(width, 0), height: Math.max(height, 0) }}>
          {width > 0 && height > 0 && <Component {...props} />}
        </AutoSizerContext.Provider>
      )}
    </AutoSizer>
  );
  Wrapped.displayName = `AutoSizerWrapped${Component.displayName}`;
  return Wrapped;
}
