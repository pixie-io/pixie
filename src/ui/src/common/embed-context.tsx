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

import { WithChildren } from 'app/utils/react-boilerplate';

/** Returns true if Pixie appears to be in an iframe or is otherwise embedded. */
export function isPixieEmbedded(): boolean {
  try {
    const isInFrame = globalThis.self !== globalThis.top;
    const isCypress = !!(window as any).Cypress; // Make an exception for the testing framework

    return isInFrame && !isCypress;
  } catch (_) {
    return true;
  }
}

export interface EmbedContextProps {
  timeArg: string;
  setTimeArg: (time: string) => void;
}

export const EmbedContext = React.createContext<EmbedContextProps>(null);
EmbedContext.displayName = 'EmbedContext';

export const EmbedContextProvider = React.memo<WithChildren>(({ children }) => {
  const [timeArg, setTimeArg] = React.useState<string>('');

  return (
    <EmbedContext.Provider value={{ timeArg, setTimeArg }}>
      {children}
    </EmbedContext.Provider>
  );
});
EmbedContextProvider.displayName = 'EmbedContextProvider';
