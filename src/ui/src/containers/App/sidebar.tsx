/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import AnnounceKit from 'announcekit-react';
import * as React from 'react';

import Drawer from '@material-ui/core/Drawer';
import {
  alpha, withStyles, Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

import HelpIcon from '@material-ui/icons/Help';
import List from '@material-ui/core/List';
import ListItem from '@material-ui/core/ListItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import AnnouncementIcon from '@material-ui/icons/Announcement';
import Menu from '@material-ui/icons/Menu';
import MenuItem from '@material-ui/core/MenuItem';
import Tooltip from '@material-ui/core/Tooltip';

import { Link } from 'react-router-dom';

import { ClusterContext } from 'common/cluster-context';
import UserContext from 'common/user-context';
import KeyboardIcon from '@material-ui/icons/Keyboard';
import {
  Avatar, ProfileMenuWrapper,
  ClusterIcon, CodeIcon, DocsIcon,
  LogoutIcon, NamespaceIcon, SettingsIcon,
  PixieLogo,
} from '@pixie-labs/components';
import { toEntityPathname, LiveViewPage } from 'containers/live-widgets/utils/live-view-params';
import {
  DOMAIN_NAME, ANNOUNCEMENT_ENABLED,
  ANNOUNCE_WIDGET_URL, CONTACT_ENABLED,
} from 'containers/constants';
import { LiveShortcutsContext } from 'containers/live/shortcuts';
import { SidebarContext } from 'context/sidebar-context';
import { LiveTourContext, LiveTourDialog } from 'containers/App/live-tour';
import ExploreIcon from '@material-ui/icons/Explore';
import { useSetting, useUserInfo } from '@pixie-labs/api-react';
import { LayoutContext } from 'context/layout-context';
import { Button } from '@material-ui/core';

const styles = (
  {
    spacing,
    palette,
    transitions,
    breakpoints,
  }: Theme) => createStyles({
  announcekit: {
    '& .announcekit-widget-badge': {
      position: 'absolute !important',
      top: spacing(2),
      left: spacing(4),
    },
  },
  avatarSm: {
    backgroundColor: palette.primary.main,
    width: spacing(4),
    height: spacing(4),
    alignItems: 'center',
  },
  avatarLg: {
    backgroundColor: palette.primary.main,
    width: spacing(7),
    height: spacing(7),
  },
  divider: {
    backgroundColor: palette.foreground.grey2,
  },
  docked: {
    position: 'absolute',
  },
  drawerClose: {
    border: 'none',
    transition: transitions.create('width', {
      easing: transitions.easing.sharp,
      duration: transitions.duration.leavingScreen,
    }),
    width: spacing(6),
    zIndex: 1300,
    overflowX: 'hidden',
    backgroundColor: palette.sideBar.color,
    boxShadow: `${spacing(0.25)}px 0px ${spacing(1)}px `
      + `${alpha(palette.sideBar.colorShadow, palette.sideBar.colorShadowOpacity)}`,
    borderRight: palette.border.unFocused,
    paddingBottom: spacing(2),
    [breakpoints.down('sm')]: {
      display: 'none',
    },
  },
  compactHamburger: {
    display: 'none',
    [breakpoints.down('sm')]: {
      paddingTop: spacing(1),
      display: 'block',
    },
  },
  drawerOpen: {
    border: 'none',
    width: spacing(29),
    zIndex: 1300,
    flexShrink: 0,
    whiteSpace: 'nowrap',
    transition: transitions.create('width', {
      easing: transitions.easing.sharp,
      duration: transitions.duration.enteringScreen,
    }),
    overflowX: 'hidden',
    backgroundColor: palette.sideBar.color,
    boxShadow: `${spacing(0.25)}px 0px ${spacing(1)}px `
      + `${alpha(palette.sideBar.colorShadow, palette.sideBar.colorShadowOpacity)}`,
    paddingBottom: spacing(2),
  },
  expandedProfile: {
    flexDirection: 'column',
  },
  icon: {
    color: palette.foreground.white,
    fill: palette.foreground.white,
  },
  listIcon: {
    paddingLeft: spacing(1.5),
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
  },
  pixieLogo: {
    fill: palette.primary.main,
    width: '48px',
  },
  profileIcon: {
    paddingLeft: spacing(1),
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
  },
  profileText: {
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    overflow: 'hidden',
    marginLeft: spacing(0.5),
  },
  sidebarToggleIcon: {
    color: palette.foreground.two,
  },
  sidebarToggle: {
    position: 'absolute',
    width: spacing(6),
    left: 0,
  },
  sidebarToggleSpacer: {
    width: spacing(6),
  },
  spacer: {
    flex: 1,
  },
  hideOnMobile: {
    // Same breakpoint (960px) at which the entire layout switches to suit mobile.
    [breakpoints.down('sm')]: {
      display: 'none',
    },
    width: '100%',
  },
});

const SideBarInternalLinkItem = ({
  classes, icon, link, text,
}) => (
  <Tooltip title={text}>
    <ListItem button component={Link} to={link} key={text} className={classes.listIcon}>
      <ListItemIcon>{icon}</ListItemIcon>
      <ListItemText primary={text} />
    </ListItem>
  </Tooltip>
);

const SideBarExternalLinkItem = ({
  classes, icon, link, text,
}) => (
  <Tooltip title={text}>
    <ListItem button component='a' href={link} key={text} className={classes.listIcon} target='_blank'>
      <ListItemIcon>{icon}</ListItemIcon>
      <ListItemText primary={text} />
    </ListItem>
  </Tooltip>
);

const StyledListItemText = withStyles((theme: Theme) => createStyles({
  primary: {
    ...theme.typography.body2,
    color: theme.palette.text.primary,
  },
}))(ListItemText);

const StyledListItemIcon = withStyles(() => createStyles({
  root: {
    minWidth: '30px',
  },
}))(ListItemIcon);

const ProfileItem = ({
  classes, userInfo, setSidebarOpen,
}) => {
  const [open, setOpen] = React.useState<boolean>(false);
  const { setTourOpen } = React.useContext(LiveTourContext);
  const [tourSeen, setTourSeen, loadingTourSeen] = useSetting('tourSeen');
  const [wasSidebarOpenBeforeTour, setWasSidebarOpenBeforeTour] = React.useState<boolean>(false);
  const [wasDrawerOpenBeforeTour, setWasDrawerOpenBeforeTour] = React.useState<boolean>(false);
  const { setDataDrawerOpen } = React.useContext(LayoutContext) ?? { setDataDrawerOpen: () => {} };
  const [anchorEl, setAnchorEl] = React.useState(null);
  const shortcuts = React.useContext(LiveShortcutsContext);
  const { inLiveView } = React.useContext(SidebarContext);

  const openMenu = React.useCallback((event) => {
    setOpen(true);
    setAnchorEl(event.currentTarget);
  }, []);

  const closeMenu = React.useCallback(() => {
    setOpen(false);
    setAnchorEl(null);
  }, []);

  const openTour = () => {
    setTourOpen(true);
    setSidebarOpen((current) => {
      setWasSidebarOpenBeforeTour(current);
      return false;
    });
    setDataDrawerOpen((current) => {
      setWasDrawerOpenBeforeTour(current);
      return false;
    });
  };

  const closeTour = () => {
    setTourOpen(false);
    setSidebarOpen(wasSidebarOpenBeforeTour);
    setDataDrawerOpen(wasDrawerOpenBeforeTour);
  };

  React.useEffect(() => {
    if (!loadingTourSeen && tourSeen !== true && inLiveView) {
      openTour();
      setTourSeen(true);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [loadingTourSeen, tourSeen, inLiveView]);

  let name = '';
  let picture = '';
  let email = '';
  let id = '';
  if (userInfo) {
    ({
      name, picture, email, id,
    } = userInfo);
  }

  React.useEffect(() => {
    if (id) {
      analytics.identify(id, { email });
    }
  }, [id, email]);

  return (
    <>
      <LiveTourDialog onClose={closeTour} />
      <ListItem button key='Profile' className={classes.profileIcon} onClick={openMenu}>
        <ListItemIcon>
          <Avatar
            name={name}
            picture={picture}
            className={classes.avatarSm}
          />
        </ListItemIcon>
        <ListItemText
          primary={name}
          secondary={email}
          classes={{ primary: classes.profileText, secondary: classes.profileText }}
        />
      </ListItem>
      <ProfileMenuWrapper
        classes={classes}
        open={open}
        onCloseMenu={closeMenu}
        anchorEl={anchorEl}
        anchorOrigin={{ vertical: 'bottom', horizontal: 'right' }}
        name={name}
        email={email}
        picture={picture}
      >
        <MenuItem key='admin' button component={Link} to='/admin'>
          <StyledListItemIcon>
            <SettingsIcon />
          </StyledListItemIcon>
          <StyledListItemText primary='Admin' />
        </MenuItem>
        {
          inLiveView && (
            [
              (
                <MenuItem key='tour' button component='button' onClick={openTour} className={classes.hideOnMobile}>
                  <StyledListItemIcon>
                    <ExploreIcon />
                  </StyledListItemIcon>
                  <StyledListItemText primary='Tour' />
                </MenuItem>
              ),
              (
                <MenuItem key='shortcuts' button component='button' onClick={() => shortcuts['show-help'].handler()}>
                  <StyledListItemIcon>
                    <KeyboardIcon />
                  </StyledListItemIcon>
                  <StyledListItemText primary='Keyboard Shortcuts' />
                </MenuItem>
              ),
            ]
          )
        }
        <MenuItem key='credits' button component={Link} to='/credits'>
          <StyledListItemIcon>
            <CodeIcon />
          </StyledListItemIcon>
          <StyledListItemText primary='Credits' />
        </MenuItem>
        <MenuItem key='logout' button component={Link} to='/logout'>
          <StyledListItemIcon>
            <LogoutIcon />
          </StyledListItemIcon>
          <StyledListItemText primary='Logout' />
        </MenuItem>
      </ProfileMenuWrapper>
    </>
  );
};

const HamburgerMenu = ({ classes, onToggle, logoLinkTo }) => (
  <ListItem button onClick={onToggle} key='Menu' className={classes.listIcon}>
    <ListItemIcon>
      <Menu className={classes.icon} />
    </ListItemIcon>
    <ListItemIcon>
      <Button
        component={Link}
        disabled={window.location.pathname.startsWith(logoLinkTo)}
        to={logoLinkTo}
        variant='text'
      >
        <PixieLogo className={classes.pixieLogo} />
      </Button>
    </ListItemIcon>
  </ListItem>
);

const SideBar = ({ classes }) => {
  const [sidebarOpen, setSidebarOpen] = React.useState<boolean>(false);
  const toggleSidebar = React.useCallback(() => setSidebarOpen((opened) => !opened), []);

  const { selectedClusterName } = React.useContext(ClusterContext);
  const { user } = React.useContext(UserContext);
  const [{ user: userInfo }] = useUserInfo();

  const navItems = React.useMemo(() => (
    [{
      icon: <ClusterIcon className={classes.icon} />,
      link: toEntityPathname({ params: {}, clusterName: selectedClusterName, page: LiveViewPage.Cluster }),
      text: 'Cluster',
    },
    {
      icon: <NamespaceIcon className={classes.icon} />,
      link: toEntityPathname({ params: {}, clusterName: selectedClusterName, page: LiveViewPage.Namespaces }),
      text: 'Namespaces',
    }]
    // eslint-disable-next-line react-hooks/exhaustive-deps
  ), [selectedClusterName]);

  return (
    <>
      <div className={classes.compactHamburger}>
        <ListItem button onClick={toggleSidebar} key='Menu' className={classes.listIcon}>
          <ListItemIcon>
            <Menu className={classes.icon} />
          </ListItemIcon>
        </ListItem>
      </div>
      <Drawer
        variant='permanent'
        className={sidebarOpen ? classes.drawerOpen : classes.drawerClose}
        classes={{
          paper: sidebarOpen ? classes.drawerOpen : classes.drawerClose,
          docked: classes.docked,
        }}
      >
        <List>
          <HamburgerMenu key='Menu' classes={classes} onToggle={toggleSidebar} logoLinkTo='/live' />
        </List>
        <List>
          {navItems.map(({ icon, link, text }) => (
            <SideBarInternalLinkItem key={text} classes={classes} icon={icon} link={link} text={text} />
          ))}
        </List>
        <div className={classes.spacer} />
        <List>
          <Tooltip title='Announcements'>
            <div className={classes.announcekit}>
              {
                ANNOUNCEMENT_ENABLED && (
                <AnnounceKit
                  widget={ANNOUNCE_WIDGET_URL}
                  user={
                        {
                          id: user.email,
                          email: user.email,
                        }
                 }
                  data={
                        {
                          org: user.orgName,
                        }
                   }
                >
                  <ListItem button key='announcements' className={classes.listIcon}>
                    <ListItemIcon><AnnouncementIcon className={classes.icon} /></ListItemIcon>
                    <ListItemText primary='Announcements' />
                  </ListItem>
                </AnnounceKit>
                )
              }
            </div>
          </Tooltip>
          <SideBarExternalLinkItem
            key='Docs'
            classes={classes}
            icon={<DocsIcon className={classes.icon} />}
            link={`https://docs.${DOMAIN_NAME}`}
            text='Docs'
          />
          { CONTACT_ENABLED && (
            <Tooltip title='Help'>
              <ListItem button id='intercom-trigger' className={classes.listIcon}>
                <ListItemIcon><HelpIcon className={classes.icon} /></ListItemIcon>
                <ListItemText primary='Help' />
              </ListItem>
            </Tooltip>
          )}
          <ProfileItem classes={classes} userInfo={userInfo} setSidebarOpen={setSidebarOpen} />
        </List>
      </Drawer>
    </>
  );
};

export default withStyles(styles)(SideBar);
