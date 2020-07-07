import * as React from 'react';
import { render } from 'enzyme';
import { createMuiTheme, MuiThemeProvider } from '@material-ui/core';
import { FixedSizeDrawer } from './drawer';

// TODO(michelle): Currently doesn't work with the clsx import. Enable when we've solved that problem.
xdescribe('<FixedSizeDrawer/>', () => {
  it('renders correctly when closed', () => {
    const otherContent = (
      <div>
        Other content. Some text goes here.
      </div>
    );

    const wrapper = render(
      <MuiThemeProvider theme={createMuiTheme()}>
        <FixedSizeDrawer
          drawerDirection='left'
          drawerSize='50px'
          open={false}
          otherContent={otherContent}
        >
          <div>
            Drawer contents
          </div>
        </FixedSizeDrawer>
      </MuiThemeProvider>,
    );
    expect(wrapper).toMatchSnapshot();
  });
});
