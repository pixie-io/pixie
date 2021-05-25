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
import { PropsWithChildren } from 'react';
import { Theme, ThemeProvider } from '@material-ui/core/styles';
import { DARK_THEME } from '@pixie-labs/components';
import { LayoutContext, LayoutContextProps } from 'context/layout-context';
import { LiveTourContext, LiveTourContextProps } from 'containers/App/live-tour';
import { ResultsContext, ResultsContextProps } from 'context/results-context';
import { ScriptContext, ScriptContextProps } from 'context/script-context';
import { ScriptsContext, ScriptsContextProps } from 'containers/App/scripts-context';
import { ClusterContext, ClusterContextProps } from 'common/cluster-context';
import VizierGRPCClientContext, { VizierGRPCClientContextProps } from 'common/vizier-grpc-client-context';
import { GQLClusterStatus as ClusterStatus } from '@pixie-labs/api';
// TODO(nick,PC-738): Fix this import once the corresponding export is corrected so it doesn't get bundled in the root.
import { MockPixieAPIContextProvider } from '@pixie-labs/api-react/src/testing';

interface MockProps {
  theme?: Theme;
  vizier?: VizierGRPCClientContextProps;
  layout?: LayoutContextProps;
  liveTour?: LiveTourContextProps;
  results?: ResultsContextProps;
  script?: ScriptContextProps;
  scripts?: ScriptsContextProps;
  cluster?: ClusterContextProps;
}

export const LIVE_CONTEXT_DEFAULTS: Required<MockProps> = {
  theme: DARK_THEME,
  vizier: {
    client: null,
    healthy: true,
    loading: false,
    clusterStatus: ClusterStatus.CS_HEALTHY,
  },
  layout: {
    editorSplitsSizes: [40, 60],
    editorPanelOpen: false,
    setEditorSplitSizes: jest.fn(),
    setEditorPanelOpen: jest.fn(),
    dataDrawerSplitsSizes: [60, 40],
    dataDrawerOpen: false,
    setDataDrawerSplitsSizes: jest.fn(),
    setDataDrawerOpen: jest.fn(),
    isMobile: false,
  },
  liveTour: {
    tourOpen: false,
    setTourOpen: jest.fn(),
  },
  results: {
    clearResults: jest.fn(),
    setResults: jest.fn(),
    setLoading: jest.fn(),
    setStreaming: jest.fn(),
    loading: false,
    streaming: false,
    tables: {},
  },
  script: {
    script: {
      id: '',
      title: '',
      code: '',
      visString: '',
      vis: {
        variables: [],
        widgets: [],
        globalFuncs: [],
      },
    },
    args: {},
    setScriptAndArgs: jest.fn(),
    setScriptAndArgsManually: jest.fn(),
    manual: false,
    execute: jest.fn(),
    cancelExecution: jest.fn(),
  },
  scripts: {
    scripts: new Map(),
    loading: false,
    setScratchScript: jest.fn(),
  },
  cluster: {
    selectedCluster: '',
    selectedClusterUID: '',
    selectedClusterName: '',
    selectedClusterPrettyName: '',
    setCluster: jest.fn(),
  },
};

function get<K extends keyof MockProps>(props: MockProps, context: K): MockProps[K] {
  return props[context] ?? LIVE_CONTEXT_DEFAULTS[context];
}

type CompType = React.FC<PropsWithChildren<MockProps>>;

/**
 * A wrapper to provide default context values for everything commonly used by Live View components. Every context can
 * be overridden in specific tests by specifying this component's props, or by nesting another Provider.
 */
export const MockLiveContextProvider: CompType = ({ children, ...props }) => (
  <ThemeProvider theme={get(props, 'theme')}>
    <MockPixieAPIContextProvider>
      <VizierGRPCClientContext.Provider value={get(props, 'vizier')}>
        <LayoutContext.Provider value={get(props, 'layout')}>
          <LiveTourContext.Provider value={get(props, 'liveTour')}>
            <ClusterContext.Provider value={get(props, 'cluster')}>
              <ResultsContext.Provider value={get(props, 'results')}>
                <ScriptsContext.Provider value={get(props, 'scripts')}>
                  <ScriptContext.Provider value={get(props, 'script')}>
                    {children}
                  </ScriptContext.Provider>
                </ScriptsContext.Provider>
              </ResultsContext.Provider>
            </ClusterContext.Provider>
          </LiveTourContext.Provider>
        </LayoutContext.Provider>
      </VizierGRPCClientContext.Provider>
    </MockPixieAPIContextProvider>
  </ThemeProvider>
);
