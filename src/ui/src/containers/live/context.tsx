import * as React from 'react';

import { ArgsContextProvider } from './context/args-context';
import { DataDrawerContextProvider } from './context/data-drawer-context';
import { ExeucteContextProvider } from './context/execute-context';
import { LayoutContextProvider } from './context/layout-context';
import { ResultsContextProvider } from './context/results-context';
import { ScriptContextProvider } from './context/script-context';
import { VisContextProvider } from './context/vis-context';

export const withLiveViewContext = (Component) => {
  return function LiveViewContextProvider() {
    return (
      <LayoutContextProvider>
        <DataDrawerContextProvider>
          <ScriptContextProvider>
            <VisContextProvider>
              <ArgsContextProvider>
                <ResultsContextProvider>
                  <ExeucteContextProvider>
                    <Component />
                  </ExeucteContextProvider>
                </ResultsContextProvider>
              </ArgsContextProvider>
            </VisContextProvider>
          </ScriptContextProvider>
        </DataDrawerContextProvider>
      </LayoutContextProvider>
    );
  };
};
