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
AutoSizerContext.displayName = 'AutoSizerContext';

const AutoSizerContextProvider: React.FC<{
  width: number, height: number, content: React.ReactNode,
}> = React.memo(
  function AutoSizerContextProvider({ width, height, content }) {
    const value = React.useMemo(() => ({ width: Math.max(width, 0), height: Math.max(height, 0) }), [width, height]);
    return (
      <AutoSizerContext.Provider value={value}>
        {width > 0 && height > 0 && content}
      </AutoSizerContext.Provider>
    );
  },
);
AutoSizerContextProvider.displayName = 'AutoSizerContextProvider';

/** Wraps component in an <AutoSizer>, and provides the width/height properties that creates in AutoSizerContext. */
export function withAutoSizerContext<P>(Component: React.ComponentType<P>): React.ComponentType<P> {
  // Note: no memoization in this component, as it's wrapping a component that may not be pure.
  // eslint-disable-next-line react-memo/require-memo
  const Wrapped: React.FC<P> = (props) => (
    <AutoSizer>
      {({ width, height }) => (
        // eslint-disable-next-line react-memo/require-usememo
        <AutoSizerContextProvider width={width} height={height} content={<Component {...props} />} />
      )}
    </AutoSizer>
  );
  Wrapped.displayName = Component.displayName ? `AutoSizerWrapped${Component.displayName}` : 'AutoSizerWrapper';
  return Wrapped;
}
