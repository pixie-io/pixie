import * as React from 'react';
import { PropsWithChildren } from 'react';
import { Theme, ThemeProvider } from '@material-ui/core/styles';
import { DARK_THEME } from 'pixie-components';
import { MockedProvider as MockApolloProvider, MockedProviderProps as MockApolloProps } from '@apollo/react-testing';
import { LayoutContext, LayoutContextProps } from 'context/layout-context';
import { LiveTourContext, LiveTourContextProps } from 'containers/App/live-tour';
import { ResultsContext, ResultsContextProps } from 'context/results-context';
import { ScriptContext, ScriptContextProps } from 'context/script-context';
import { ScriptsContext, ScriptsContextProps } from 'containers/App/scripts-context';
import { LiveViewPage } from 'containers/live-widgets/utils/live-view-params';
import { ClusterContext, ClusterContextProps } from 'common/cluster-context';

interface MockProps {
  theme?: Theme;
  apollo?: MockApolloProps;
  layout?: LayoutContextProps;
  liveTour?: LiveTourContextProps;
  results?: ResultsContextProps;
  script?: ScriptContextProps;
  scripts?: ScriptsContextProps;
  cluster?: ClusterContextProps;
}

const defaults: Required<MockProps> = {
  theme: DARK_THEME,
  apollo: {},
  layout: {
    editorSplitsSizes: [50, 50],
    editorPanelOpen: false,
    setEditorSplitSizes: jest.fn(),
    setEditorPanelOpen: jest.fn(),
    dataDrawerSplitsSizes: [50, 50],
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
    liveViewPage: LiveViewPage.Default,
    args: {},
    setArgs: jest.fn(),
    visJSON: '',
    vis: {
      variables: [],
      widgets: [],
      globalFuncs: [],
    },
    setVis: jest.fn(),
    setCancelExecution: jest.fn(),
    pxlEditorText: '',
    visEditorText: '',
    setVisEditorText: jest.fn(),
    setPxlEditorText: jest.fn(),
    pxl: '',
    setPxl: jest.fn(),
    title: '',
    id: '',
    setScript: jest.fn(),
    execute: jest.fn(),
    saveEditorAndExecute: jest.fn(),
    parseVisOrShowError: jest.fn(),
    argsForVisOrShowError: jest.fn(),
  },
  scripts: {
    scripts: new Map(),
    promise: Promise.resolve(new Map()),
  },
  cluster: {
    selectedCluster: '',
    selectedClusterUID: '',
    selectedClusterName: '',
    selectedClusterPrettyName: '',
    setCluster: jest.fn(),
    setClusterByName: jest.fn(),
  },
};

function get<K extends keyof MockProps>(props: MockProps, context: K): MockProps[K] {
  return props[context] ?? defaults[context];
}

type CompType = React.FC<PropsWithChildren<MockProps>>;

/**
 * A wrapper to provide default context values for everything commonly used by Live View components. Every context can
 * be overridden in specific tests by specifying this component's props, or by nesting another Provider.
 */
export const MockLiveContextProvider: CompType = ({ children, ...props }) => (
  <ThemeProvider theme={get(props, 'theme')}>
    <MockApolloProvider {...get(props, 'apollo')}>
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
    </MockApolloProvider>
  </ThemeProvider>
);
