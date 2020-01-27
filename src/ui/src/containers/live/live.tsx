import {DARK_THEME} from 'common/mui-theme';
import * as React from 'react';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import {createStyles, makeStyles, Theme, ThemeProvider} from '@material-ui/core/styles';
import EditIcon from '@material-ui/icons/Edit';
import MenuIcon from '@material-ui/icons/Menu';
import ShareIcon from '@material-ui/icons/Share';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      width: '100%',
    },
    topBar: {
      display: 'flex',
      margin: theme.spacing(2),
      alignItems: 'center',
    },
    title: {
      marginLeft: theme.spacing(2),
      flexGrow: 1,
    },
  }));

const LiveView = () => {
  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = (open: boolean) => () => { setDrawerOpen(open); };

  const classes = useStyles();

  return (
    <div className={classes.root}>
      <div className={classes.topBar}>
        <IconButton onClick={toggleDrawer(true)}>
          <MenuIcon />
        </IconButton>
        <div className={classes.title}>title goes here</div>
        <IconButton>
          <ShareIcon />
        </IconButton>
        <IconButton>
          <EditIcon />
        </IconButton>
      </div>
      <div>Live view goes here</div>
      <Drawer open={drawerOpen} onClose={toggleDrawer(false)}>
        <div>drawer content</div>
      </Drawer>
    </div>
  );
};

export default () => (
  // Wrap the component with the theme provider here for now.
  // TODO(malthus): Remove this once the whole app switches to MUI.
  <ThemeProvider theme={DARK_THEME}>
    <LiveView />
  </ThemeProvider>
);
