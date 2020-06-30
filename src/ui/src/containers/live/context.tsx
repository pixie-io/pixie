import * as React from 'react';

import { DataDrawerContextProvider } from 'context/data-drawer-context';
import { LayoutContextProvider } from 'context/layout-context';
import { ResultsContextProvider } from 'context/results-context';
import ScriptContextProvider from 'context/script-context';

export const withLiveViewContext = (Component) => function LiveViewContextProvider() {
  return (
    <LayoutContextProvider>
      <DataDrawerContextProvider>
        <ResultsContextProvider>
          <ScriptContextProvider>
            <Component />
          </ScriptContextProvider>
        </ResultsContextProvider>
      </DataDrawerContextProvider>
    </LayoutContextProvider>
  );
};
