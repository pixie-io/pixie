import * as React from 'react';
import Drawer from '@material-ui/core/Drawer';
import {
  createStyles, WithStyles, withStyles, Theme,
} from '@material-ui/core/styles';

import ClusterIcon from 'components/icons/cluster';
import NamespaceIcon from 'components/icons/namespace';
import List from '@material-ui/core/List';
import ListItem from '@material-ui/core/ListItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import ProfileMenu from 'containers/profile-menu/profile-menu';
import SettingsIcon from 'components/icons/settings';
import { Link } from 'react-router-dom';
import Tooltip from '@material-ui/core/Tooltip';

const styles = ({ spacing, palette, transitions }: Theme) => createStyles({
  drawerOpen: {
    width: spacing(28),
    zIndex: 1250,
    flexShrink: 0,
    whiteSpace: 'nowrap',
    paddingTop: spacing(8),
    transition: transitions.create('width', {
      easing: transitions.easing.sharp,
      duration: transitions.duration.enteringScreen,
    }),
    overflowX: 'hidden',
  },
  drawerClose: {
    transition: transitions.create('width', {
      easing: transitions.easing.sharp,
      duration: transitions.duration.leavingScreen,
    }),
    width: spacing(8),
    zIndex: 1250,
    overflowX: 'hidden',
    paddingTop: spacing(8),
  },
  docked: {
    position: 'absolute',
  },
  spacer: {
    flex: 1,
  },
  listIcon: {
    paddingLeft: spacing(2.3),
  },
  divider: {
    backgroundColor: palette.foreground.grey2,
  },
});

interface SideBarProps extends WithStyles<typeof styles> {
  open: boolean;
}

const sideBarItems = [{
  icon: <ClusterIcon />,
  link: '/script?script=px%2Fcluster&start_time=-5m',
  text: 'Clusters',
},
{
  icon: <NamespaceIcon />,
  link: '/script?script=px%2Fnamespaces&start_time=-5m',
  text: 'Namespaces',
}];

const profileItems = [
  {
    icon: <SettingsIcon />,
    link: '/admin',
    text: 'Admin',
  },
];

const SideBarItem = ({
  classes, icon, link, text,
}) => (
  <Tooltip title={text}>
    <ListItem button component={Link} to={link} key={text} className={classes.listIcon}>
      <ListItemIcon>{icon}</ListItemIcon>
      <ListItemText primary={text} />
    </ListItem>
  </Tooltip>
);

const SideBar = ({
  classes, open,
}) => (
  <Drawer
    variant='permanent'
    className={open ? classes.drawerOpen : classes.drawerClose}
    classes={{
      paper: open ? classes.drawerOpen : classes.drawerClose,
      docked: classes.docked,
    }}
  >
    <List>
      {sideBarItems.map(({ icon, link, text }) => (
        <SideBarItem key={text} classes={classes} icon={icon} link={link} text={text} />
      ))}
    </List>
    <div className={classes.spacer} />
    <List>
      {profileItems.map(({ icon, link, text }) => (
        <SideBarItem key={text} classes={classes} icon={icon} link={link} text={text} />
      ))}
    </List>
    <div>
      <ProfileMenu />
    </div>
  </Drawer>
);

export default withStyles(styles)(SideBar);
