import * as React from 'react';
import {
  createStyles, WithStyles, withStyles, Theme,
} from '@material-ui/core/styles';

import AppBar from '@material-ui/core/AppBar';
import Toolbar from '@material-ui/core/Toolbar';
import Menu from '@material-ui/icons/Menu';
import IconButton from '@material-ui/core/IconButton';

import SideBar from 'containers/App/sidebar';

const styles = ({ spacing, palette }: Theme) => createStyles({
  appBar: {
    zIndex: 1300, // z-index must be larger than drawer's zIndex, which is 1200.
    background: palette.background.one,
  },
  sidebarToggle: {
    position: 'absolute',
    width: spacing(8),
    left: 0,
  },
  sidebarToggleSpacer: {
    width: spacing(8),
  },
});

interface NavBarsProps extends WithStyles<typeof styles> {
  appBarContents: React.ReactNode;
  children?: React.ReactNode;
}

const NavBars = ({
  classes, children,
}) => {
  const [sidebarOpen, setSidebarOpen] = React.useState<boolean>(false);
  const toggleSidebar = React.useCallback(() => setSidebarOpen((opened) => !opened), []);

  return (
    <>
      <AppBar className={classes.appBar} color='transparent' position='static'>
        <Toolbar>
          <IconButton className={classes.sidebarToggle} onClick={toggleSidebar}>
            <Menu />
          </IconButton>
          <div className={classes.sidebarToggleSpacer} />
          {children}
        </Toolbar>
      </AppBar>
      <SideBar open={sidebarOpen} />
    </>
  );
};

export default withStyles(styles)(NavBars);
