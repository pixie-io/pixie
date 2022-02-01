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

import * as React from 'react';

import { useQuery, useMutation, gql } from '@apollo/client';
import {
  Explore as ExploreIcon,
  Keyboard as KeyboardIcon,
  Menu as MenuIcon,
} from '@mui/icons-material';
import {
  AppBar,
  IconButton,
  ListItemIcon,
  ListItemText,
  MenuItem,
  Toolbar,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Link } from 'react-router-dom';

import OrgContext from 'app/common/org-context';
import {
  Avatar,
  ProfileMenuWrapper,
  LogoutIcon,
  SettingsIcon,
} from 'app/components';
import { LiveTourContext, LiveTourDialog } from 'app/containers/App/live-tour';
import { LiveShortcutsContext } from 'app/containers/live/shortcuts';
import { SetStateFunc } from 'app/context/common';
import { LayoutContext } from 'app/context/layout-context';
import { SidebarContext } from 'app/context/sidebar-context';
import { GQLUserInfo, GQLUserAttributes } from 'app/types/schema';
import pixieAnalytics from 'app/utils/analytics';
import { buildClass } from 'app/utils/build-class';
import { Logo } from 'configurable/logo';

const useStyles = makeStyles((theme: Theme) => createStyles({
  container: {
    zIndex: theme.zIndex.appBar,
    backgroundColor: theme.palette.background.paper,
  },
  contents: {
    display: 'flex',
    flex: 1,
    paddingRight: theme.spacing(4),
    paddingLeft: theme.spacing(4),
    height: '100%',
    alignItems: 'center',
  },
  menu: {
    color: theme.palette.text.secondary,
  },
  clickable: {
    cursor: 'pointer',
  },
  managedDomainBanner: {
    background: theme.palette.background.one,
    fontSize: theme.typography.caption.fontSize,
    fontStyle: 'italic',
    opacity: 0.8,
    padding: theme.spacing(1),
    margin: `0 ${theme.spacing(1)}`,
    width: 'auto', // For flex calculations with margins
    textAlign: 'center',
  },
  expandedProfile: {
    alignItems: 'center',
    padding: `${theme.spacing(0.75)} ${theme.spacing(2)}`,
  },
  menuItem: {
    ...theme.typography.body2,
    paddingTop: theme.spacing(1),
    paddingBottom: theme.spacing(1),
  },
  menuItemIcon: {
    '&.MuiListItemIcon-root': { // Precedence
      minWidth: theme.spacing(3.75),
    },
  },
  menuItemText: {
    margin: `${theme.spacing(0.5)} 0`,
  },
  avatarSm: {
    backgroundColor: theme.palette.primary.main,
    width: theme.spacing(4),
    height: theme.spacing(4),
    alignItems: 'center',
  },
  centeredListItemText: {
    paddingLeft: theme.spacing(1),
  },
  hideOnMobile: {
    // Same breakpoint (960px) at which the entire layout switches to suit mobile.
    [theme.breakpoints.down('sm')]: {
      display: 'none',
    },
    width: '100%',
  },
  profileIcon: {
    paddingLeft: theme.spacing(1),
    paddingTop: theme.spacing(1),
    paddingBottom: theme.spacing(1),
  },
}), { name: 'TopBar' });

const ProfileItem = React.memo<{ setSidebarOpen: SetStateFunc<boolean> }>(({ setSidebarOpen }) => {
  const classes = useStyles();
  const [open, setOpen] = React.useState<boolean>(false);
  const { setTourOpen } = React.useContext(LiveTourContext);
  const [wasSidebarOpenBeforeTour, setWasSidebarOpenBeforeTour] = React.useState<boolean>(false);
  const [wasDrawerOpenBeforeTour, setWasDrawerOpenBeforeTour] = React.useState<boolean>(false);
  const { setDataDrawerOpen } = React.useContext(LayoutContext) ?? { setDataDrawerOpen: () => {} };
  const [anchorEl, setAnchorEl] = React.useState(null);
  const shortcuts = React.useContext(LiveShortcutsContext);
  const { showLiveOptions, showAdmin } = React.useContext(SidebarContext);
  const { org: { domainName } } = React.useContext(OrgContext);

  const { data } = useQuery<{
    user: Pick<GQLUserInfo, 'name' | 'picture' | 'id' | 'email' >,
  }>(gql`
    query userForProfileMenu{
      user {
        id
        name
        email
        picture
      }
    }
  `, {});
  const userInfo = data?.user;
  const isSupportUser = data?.user?.email.split('@')[1] === 'pixie.support';

  const { data: dataSettings, loading: loadingTourSeen } = useQuery<{
    userAttributes: GQLUserAttributes,
  }>(gql`
    query getTourSeen{
      userAttributes {
        tourSeen
        id
      }
    }
  `, {});
  const tourSeen = isSupportUser
    || (dataSettings?.userAttributes?.tourSeen);

  const [setTourSeen] = useMutation<
  { SetUserAttributes: GQLUserAttributes }, void
  >(gql`
    mutation updateTourSeen{
      SetUserAttributes(attributes: { tourSeen: true }) {
        tourSeen
        id
      }
    }
  `);

  const openMenu = React.useCallback((event) => {
    setOpen(true);
    setAnchorEl(event.currentTarget);
  }, []);

  const closeMenu = React.useCallback(() => {
    setOpen(false);
    setAnchorEl(null);
  }, []);

  const openTour = React.useCallback(() => {
    setTourOpen(true);
    setSidebarOpen((current) => {
      setWasSidebarOpenBeforeTour(current);
      return false;
    });
    setDataDrawerOpen((current) => {
      setWasDrawerOpenBeforeTour(current);
      return false;
    });
  }, [setDataDrawerOpen, setSidebarOpen, setTourOpen]);

  const closeTour = React.useCallback(() => {
    setTourOpen(false);
    setSidebarOpen(wasSidebarOpenBeforeTour);
    setDataDrawerOpen(wasDrawerOpenBeforeTour);
  }, [setDataDrawerOpen, setSidebarOpen, setTourOpen, wasDrawerOpenBeforeTour, wasSidebarOpenBeforeTour]);

  const showShortcuts = React.useCallback(() => {
    shortcuts?.['show-help'].handler();
  }, [shortcuts]);

  React.useEffect(() => {
    if (!loadingTourSeen && tourSeen !== true && showLiveOptions) {
      openTour();
      setTourSeen().then();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [loadingTourSeen, tourSeen, showLiveOptions]);

  /* eslint-disable react-memo/require-usememo */
  let name = '';
  let picture = '';
  let email = '';
  let id = '';
  /* eslint-enable react-memo/require-usememo */
  if (userInfo) {
    ({
      name, picture, email, id,
    } = userInfo);
  }

  React.useEffect(() => {
    if (id) {
      pixieAnalytics.identify(id, { email });
    }
  }, [id, email]);

  return (
    <>
      <LiveTourDialog onClose={closeTour} />
      <div className={classes.profileIcon} onClick={openMenu}>
        <Avatar
          name={name}
          picture={picture}
          className={buildClass(classes.avatarSm, classes.clickable)}
        />
      </div>
      <ProfileMenuWrapper
        classes={classes}
        open={open}
        onCloseMenu={closeMenu}
        anchorEl={anchorEl}
        anchorOrigin={React.useMemo(() => ({ vertical: 'bottom', horizontal: 'right' }), [])}
        name={name}
        domainName={domainName}
        email={email}
        picture={picture}
      >
        {
          showAdmin && (
            <MenuItem key='admin' component={Link} to='/admin' className={classes.menuItem}>
              <ListItemIcon className={classes.menuItemIcon}>
                <SettingsIcon />
              </ListItemIcon>
              <ListItemText primary='Admin' disableTypography className={classes.menuItemText} />
            </MenuItem>
          )
        }
        {
          showLiveOptions && (
            [
              (
                <MenuItem
                  key='tour' component='a' onClick={openTour} className={`${classes.hideOnMobile} ${classes.menuItem}`}
                >
                  <ListItemIcon className={classes.menuItemIcon}>
                    <ExploreIcon />
                  </ListItemIcon>
                  <ListItemText primary='Tour' disableTypography className={classes.menuItemText} />
                </MenuItem>
              ),
              (
                <MenuItem key='shortcuts' component='a' onClick={showShortcuts} className={classes.menuItem}>
                  <ListItemIcon className={classes.menuItemIcon}>
                    <KeyboardIcon />
                  </ListItemIcon>
                  <ListItemText primary='Keyboard Shortcuts' disableTypography className={classes.menuItemText} />
                </MenuItem>
              ),
            ]
          )
        }
        <MenuItem key='logout' component={Link} to='/logout' className={classes.menuItem}>
          <ListItemIcon className={classes.menuItemIcon}>
            <LogoutIcon />
          </ListItemIcon>
          <ListItemText primary='Logout' disableTypography className={classes.menuItemText} />
        </MenuItem>
      </ProfileMenuWrapper>
    </>
  );
});
ProfileItem.displayName = 'ProfileItem';

interface TopBarProps {
  toggleSidebar: () => void;
  setSidebarOpen: SetStateFunc<boolean>;
}

export const TopBar: React.FC<TopBarProps> = React.memo(({
  children, toggleSidebar, setSidebarOpen,
}) => {
  const classes = useStyles();
  return (
    <AppBar className={classes.container} position='static'>
      <Toolbar>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <IconButton edge='start' size='large' color='inherit' aria-label='menu' sx={{ mr: 2 }} onClick={toggleSidebar}>
          <MenuIcon className={classes.menu} />
        </IconButton>
        <Link to='/'><Logo /></Link>
        <div className={classes.contents}>
          { children }
        </div>
        <ProfileItem setSidebarOpen={setSidebarOpen} />
      </Toolbar>
    </AppBar>
  );
});
TopBar.displayName = 'TopBar';
