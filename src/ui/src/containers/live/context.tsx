import * as React from 'react';

import { ArgsContextProvider } from './context/args-context';
import { DataDrawerContextProvider } from './context/data-drawer-context';
import { ExecuteContextProvider } from './context/execute-context';
import { LayoutContextProvider } from './context/layout-context';
import { ResultsContextProvider } from './context/results-context';
import RouteContextProvider from './context/route-context';
import { ScriptContextProvider } from './context/script-context';
import { VisContextProvider } from './context/vis-context';

export const withLiveViewContext = (Component) => {
  return function LiveViewContextProvider() {
    return (
      <RouteContextProvider>
        <LayoutContextProvider>
          <DataDrawerContextProvider>
            <ScriptContextProvider>
              <VisContextProvider>
                <ArgsContextProvider>
                  <ResultsContextProvider>
                    <ExecuteContextProvider>
                      <Component />
                    </ExecuteContextProvider>
                  </ResultsContextProvider>
                </ArgsContextProvider>
              </VisContextProvider>
            </ScriptContextProvider>
          </DataDrawerContextProvider>
        </LayoutContextProvider>
      </RouteContextProvider>
    );
  };
};
