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
            return [{ value: 'cluster1' }, { value: 'cluster2' }, { value: 'cluster3' }];
          }
          return [{ value: 'cluster1' }, { value: 'cluster2' }, { value: 'cluster3' }];
        },
      },
      {
        title: 'pod',
        value: 'pod-123',
        selectable: true,
        allowTyping: true,
        getListItems: async (input) => {
          if (input === '') {
            return [{ value: 'pod1' }, { value: 'pod2' }];
          }
          return [{ value: 'some pod' }, { value: 'another pod' }, { value: 'pod' }];
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
