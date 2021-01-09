import { Table } from 'common/vizier-grpc-client';
import * as React from 'react';
import { QueryExecutionStats, MutationInfo } from 'types/generated/vizier_pb';

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

export const ResultsContextProvider = (props) => {
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
      {props.children}
    </ResultsContext.Provider>
  );
};
