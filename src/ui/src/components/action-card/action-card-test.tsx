import * as React from 'react';
import { render } from 'enzyme';
import { createMuiTheme, MuiThemeProvider } from '@material-ui/core';
import ActionCard from './action-card';

jest.mock('clsx', () => ({ default: jest.fn() }));

// TODO(zasgar): Enable this afer we fix clsx import. It's likely because of importing mjs files
// incorrectly.
xdescribe('<ActionCard/>', () => {
  it('renders correctly', () => {
    const wrapper = render(
      <MuiThemeProvider theme={createMuiTheme()}>
        <ActionCard title='testing' />
      </MuiThemeProvider>,
    );
    expect(wrapper).toMatchSnapshot();
  });
});
