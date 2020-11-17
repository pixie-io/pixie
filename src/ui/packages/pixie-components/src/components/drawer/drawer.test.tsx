import * as React from 'react';
import { render } from 'enzyme';
import { createMuiTheme, MuiThemeProvider } from '@material-ui/core';
import { FixedSizeDrawer } from './drawer';

describe('<FixedSizeDrawer/>', () => {
  it('renders correctly when closed', () => {
    const otherContent = <div>Other content. Some text goes here.</div>;

    const wrapper = render(
      <MuiThemeProvider theme={createMuiTheme()}>
        <FixedSizeDrawer
          drawerDirection='left'
          drawerSize='50px'
          open={false}
          otherContent={otherContent}
          overlay={false}
        >
          <div>Drawer contents</div>
        </FixedSizeDrawer>
      </MuiThemeProvider>
    );
    expect(wrapper).toMatchSnapshot();
  });
});
