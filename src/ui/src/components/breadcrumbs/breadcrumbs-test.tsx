import * as React from 'react';
import { render } from 'enzyme';
import { MuiThemeProvider } from '@material-ui/core';
import { DARK_THEME } from 'common/mui-theme';
import Breadcrumbs from './breadcrumbs';

jest.mock('clsx', () => ({ default: jest.fn() }));

// TODO(michelle): Currently doesn't work with the clsx import. Enable when we've solved that problem.
xdescribe('<Breadcrumbs/>', () => {
  it('renders correctly', () => {
    const breadcrumbs = [
      {
        title: 'cluster',
        value: 'gke-prod',
        selectable: true,
        allowTyping: false,
        getListItems: async (input) => {
          if (input) {
            return ['cluster1', 'cluster2', 'cluster3'];
          }
          return ['cluster1', 'cluster2', 'cluster3'];
        },
      },
      {
        title: 'pod',
        value: 'pod-123',
        selectable: true,
        allowTyping: true,
        getListItems: async (input) => {
          if (input === '') {
            return ['pod1', 'pod2'];
          }
          return ['some pod', 'another pod', 'pod'];
        },
      },
      {
        title: 'script',
        value: 'px/pod',
        selectable: false,
      },
    ];

    const wrapper = render(
      <MuiThemeProvider theme={DARK_THEME}>
        <Breadcrumbs breadcrumbs={breadcrumbs} />
      </MuiThemeProvider>,
    );
    expect(wrapper).toMatchSnapshot();
  });
});
