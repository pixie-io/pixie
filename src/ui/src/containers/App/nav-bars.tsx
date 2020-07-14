import * as React from 'react';
import {
  createStyles, WithStyles, withStyles, Theme,
} from '@material-ui/core/styles';

import AppBar from '@material-ui/core/AppBar';
import Toolbar from '@material-ui/core/Toolbar';
import Menu from '@material-ui/icons/Menu';
import IconButton from '@material-ui/core/IconButton';

import SideBar from 'containers/App/sidebar';
import clsx from 'clsx';

const styles = ({ spacing, palette }: Theme) => createStyles({
  appBar: {
    zIndex: 1300, // z-index must be larger than drawer's zIndex, which is 1200.
    background: `linear-gradient(0deg, ${palette.topBar.colorTop}, ${palette.topBar.colorBottom})`,
    height: spacing(6.5),
    boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px 0px ${palette.background.five}`,
  },
  appBarSideClosed: {
    // We offset the box shadow to no overlap the sidebar when it's closed.
    boxShadow: `${spacing(7)}px ${spacing(0.25)}px ${spacing(2)}px 0px ${palette.background.five}`,
  },
  toolbar: {
    minHeight: spacing(5.8),
  },
  sidebarToggle: {
    position: 'absolute',
    width: spacing(6),
    left: 0,
  },
  sidebarToggleSpacer: {
    width: spacing(6),
  },
  icon: {
    color: palette.foreground.two,
  },
});

interface NavBarsProps extends WithStyles<typeof styles> {
  appBarContents?: React.ReactNode;
  children?: React.ReactNode;
}

const NavBars = ({
  classes, children,
}: NavBarsProps) => {
  const [sidebarOpen, setSidebarOpen] = React.useState<boolean>(false);
  const toggleSidebar = React.useCallback(() => setSidebarOpen((opened) => !opened), []);

  return (
    <>
      <AppBar
        className={clsx(
          classes.appBar,
          !sidebarOpen && classes.appBarSideClosed,
        )}
        color='transparent'
        position='static'
      >
        <Toolbar className={classes.toolbar}>
          <IconButton className={classes.sidebarToggle} onClick={toggleSidebar}>
            <Menu className={classes.icon} />
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
