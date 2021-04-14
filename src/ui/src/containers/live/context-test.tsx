import * as React from 'react';
import { LayoutContext } from 'context/layout-context';
import { DataDrawerContext } from 'context/data-drawer-context';
import { LiveTourContext } from 'containers/App/live-tour';
import { ResultsContext } from 'context/results-context';
import { ScriptContext } from 'context/script-context';
import { withLiveViewContext } from 'containers/live/context';
// eslint-disable-next-line import/no-extraneous-dependencies
import { render } from 'enzyme';
import { Router } from 'react-router';
import history from 'utils/pl-history';
import { ScriptsContext } from 'containers/App/scripts-context';
import { ClusterContext } from 'common/cluster-context';
import { LIVE_CONTEXT_DEFAULTS } from 'testing/mocks/live-context-mock';
import { ThemeProvider } from '@material-ui/core/styles';
import { VizierGRPCClientContext } from 'common/vizier-grpc-client-context';
import { SnackbarContext } from '@pixie-labs/components';
// TODO(nick,PC-738): Fix this import once the corresponding export is corrected so it doesn't get bundled in the root.
import { MockPixieAPIContextProvider } from '@pixie-labs/api-react/src/testing';

/**
 * Replaces all of the jest.fn() values with expect.any() in the mock defaults of a context.
 * @param withMockFunctions
 */
// eslint-disable-next-line @typescript-eslint/ban-types
function mockToAny(withMockFunctions: object) {
  const functionKeys = Object.keys(withMockFunctions).filter((k) => typeof withMockFunctions[k] === 'function');
  const withExpectAny = functionKeys.reduce((p, k) => ({
    ...p,
    [k]: expect.any(Function),
  }), {});
  return {
    ...withMockFunctions,
    ...withExpectAny,
  };
}

describe('Live aggregate context', () => {
  // Note: doing this runs all of the SomeContextProvider functions, which makes them count as covered by tests.
  // That's technically true, but they still need their own tests.
  it('provides defaults for every context it creates', () => {
    const Tester = withLiveViewContext(() => {
      const layout = React.useContext(LayoutContext);
      expect(layout).toEqual(mockToAny(LIVE_CONTEXT_DEFAULTS.layout));
      const dataDrawer = React.useContext(DataDrawerContext);
      expect(dataDrawer).toEqual({
        activeTab: '',
        openDrawerTab: expect.any(Function),
        setActiveTab: expect.any(Function),
      });
      const liveTour = React.useContext(LiveTourContext);
      expect(liveTour).toEqual(mockToAny(LIVE_CONTEXT_DEFAULTS.liveTour));
      const results = React.useContext(ResultsContext);
      expect(results).toEqual(mockToAny(LIVE_CONTEXT_DEFAULTS.results));
      const script = React.useContext(ScriptContext);
      expect(script).toEqual(mockToAny(LIVE_CONTEXT_DEFAULTS.script));
      return <></>;
    });

    // We can't use MockLiveViewContext here since we're testing the real ones.
    // Thus, we're wrapping only the ones that aren't included in withLiveViewContext.
    render(
      <Router history={history}>
        <ThemeProvider theme={LIVE_CONTEXT_DEFAULTS.theme}>
          <MockPixieAPIContextProvider>
            <VizierGRPCClientContext.Provider value={LIVE_CONTEXT_DEFAULTS.vizier}>
              <ClusterContext.Provider value={LIVE_CONTEXT_DEFAULTS.cluster}>
                <ScriptsContext.Provider value={LIVE_CONTEXT_DEFAULTS.scripts}>
                  <SnackbarContext.Provider value={() => {}}>
                    <Tester />
                  </SnackbarContext.Provider>
                </ScriptsContext.Provider>
              </ClusterContext.Provider>
            </VizierGRPCClientContext.Provider>
          </MockPixieAPIContextProvider>
        </ThemeProvider>
      </Router>);
  });
});
