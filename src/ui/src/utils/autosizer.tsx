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
import { AutoSizer } from 'react-virtualized';

interface AutoSizerProps {
  width: number;
  height: number;
}

export type WithAutoSizerProps<T> = T & AutoSizerProps;

export default function withAutoSizer<T>(
  WrappedComponent: React.ComponentType<T & AutoSizerProps>,
) {
  return function AutoSizerWrapper(props: T): React.ReactElement {
    return (
      <AutoSizer>
        {({ height, width }) => (
          <WrappedComponent
            width={Math.max(width, 0)}
            height={Math.max(height, 0)}
            {...props}
          />
        )}
      </AutoSizer>
    );
  };
}
