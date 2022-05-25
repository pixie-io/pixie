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

import { VizierTable } from 'app/api';
import { QueryExecutionStats, MutationInfo } from 'app/types/generated/vizierapi_pb';
import { WithChildren } from 'app/utils/react-boilerplate';

import { SetStateFunc } from './common';

interface Results {
  error?: Error;
  tables: Map<string, VizierTable>;
  stats?: QueryExecutionStats;
  mutationInfo?: MutationInfo;
}

export interface ResultsContextProps extends Results {
  clearResults: () => void;
  setResults: SetStateFunc<Results>;
  loading: boolean;
  streaming: boolean;
  setLoading: SetStateFunc<boolean>;
  setStreaming: SetStateFunc<boolean>;
}

export const ResultsContext = React.createContext<ResultsContextProps>(null);
ResultsContext.displayName = 'ResultsContext';

/**
 * When streaming queries, row batch data updates happen outside of React's lifecycle and mutate tables' contents.
 * React doesn't see these changes for the purposes of memoized props, but watching ResultsContext lets it notice.
 * Use this for things like tables that need to append data while it's still being streamed, before results finalize.
 */
export function useLatestRowCount(tableName: string): number {
  const { tables } = React.useContext(ResultsContext);
  const count = tables.get(tableName)?.rows.length ?? 0;
  // This is what actually makes React watch for the change
  React.useEffect(() => {}, [tableName, count]);
  return count;
}

export const ResultsContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const [results, setResults] = React.useState<Results>({ tables: new Map() });
  const [loading, setLoading] = React.useState(false);
  const [streaming, setStreaming] = React.useState(false);
  const clearResults = React.useCallback(() => {
    setResults({ tables: new Map() });
  }, [setResults]);

  const value = React.useMemo(() => ({
    ...results,
    setResults,
    clearResults,
    loading,
    streaming,
    setLoading,
    setStreaming,
  }), [
    results,
    setResults,
    clearResults,
    loading,
    streaming,
    setLoading,
    setStreaming,
  ]);

  return (
    <ResultsContext.Provider value={value}>
      {children}
    </ResultsContext.Provider>
  );
});
ResultsContextProvider.displayName = 'ResultsContextProvider';
