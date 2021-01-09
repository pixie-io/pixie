import { render } from 'enzyme';
import * as React from 'react';
import { MockLiveContextProvider } from 'testing/mocks/live-context-mock';
import LiveViewBreadcrumbs from './breadcrumbs';

describe('Live view breadcrumbs', () => {
  it('renders', () => {
    render(<MockLiveContextProvider><LiveViewBreadcrumbs /></MockLiveContextProvider>);
  });
});
