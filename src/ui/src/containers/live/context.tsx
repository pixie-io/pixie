import * as React from 'react';

import { DataDrawerContextProvider } from 'context/data-drawer-context';
import { LayoutContextProvider } from 'context/layout-context';
import { ResultsContextProvider } from 'context/results-context';
import ScriptContextProvider from 'context/script-context';
import { LiveTourContextProvider } from 'containers/App/live-tour';

export const withLiveViewContext = (Component) => function LiveViewContextProvider() {
  return (
    <LayoutContextProvider>
      <DataDrawerContextProvider>
        <LiveTourContextProvider>
          <ResultsContextProvider>
            <ScriptContextProvider>
              <Component />
            </ScriptContextProvider>
          </ResultsContextProvider>
        </LiveTourContextProvider>
      </DataDrawerContextProvider>
    </LayoutContextProvider>
  );
};
