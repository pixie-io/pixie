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
import { Table } from 'app/api';
import { QueryExecutionStats, MutationInfo } from 'app/types/generated/vizierapi_pb';

import { SetStateFunc } from './common';

interface Tables {
  [name: string]: Table;
}

interface Results {
  error?: Error;
  tables: Tables;
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

export const ResultsContextProvider: React.FC = ({ children }) => {
  const [results, setResults] = React.useState<Results>({ tables: {} });
  const [loading, setLoading] = React.useState(false);
  const [streaming, setStreaming] = React.useState(false);
  const clearResults = () => {
    setResults({ tables: {} });
  };

  return (
    <ResultsContext.Provider value={{
      ...results,
      setResults,
      clearResults,
      loading,
      streaming,
      setLoading,
      setStreaming,
    }}
    >
      {children}
    </ResultsContext.Provider>
  );
};
