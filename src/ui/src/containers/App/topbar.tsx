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
import AppBar from '@material-ui/core/AppBar';
import { buildClass } from 'app/utils/build-class';

import Toolbar from '@material-ui/core/Toolbar';
import IconButton from '@material-ui/core/IconButton';
import MenuIcon from '@material-ui/icons/Menu';
import {
  withStyles, Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import {
  Avatar, ProfileMenuWrapper, CodeIcon,
  LogoutIcon, SettingsIcon,
} from 'app/components';
import { useQuery, useMutation, gql } from '@apollo/client';
import { LiveShortcutsContext } from 'app/containers/live/shortcuts';
import { SidebarContext } from 'app/context/sidebar-context';
import { LiveTourContext, LiveTourDialog } from 'app/containers/App/live-tour';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import { LayoutContext } from 'app/context/layout-context';
import MenuItem from '@material-ui/core/MenuItem';
import ExploreIcon from '@material-ui/icons/Explore';
import KeyboardIcon from '@material-ui/icons/Keyboard';
import { Link } from 'react-router-dom';
import { Logo } from 'configurable/logo';
import { GQLUserInfo, GQLUserAttributes } from 'app/types/schema';

const StyledListItemText = withStyles((theme: Theme) => createStyles({
  primary: {
    ...theme.typography.body2,
  },
}))(ListItemText);

const StyledListItemIcon = withStyles(() => createStyles({
  root: {
    minWidth: '30px',
  },
}))(ListItemIcon);

const ProfileItem = ({
  classes, setSidebarOpen,
}) => {
  const [open, setOpen] = React.useState<boolean>(false);
  const { setTourOpen } = React.useContext(LiveTourContext);
  const [wasSidebarOpenBeforeTour, setWasSidebarOpenBeforeTour] = React.useState<boolean>(false);
  const [wasDrawerOpenBeforeTour, setWasDrawerOpenBeforeTour] = React.useState<boolean>(false);
  const { setDataDrawerOpen } = React.useContext(LayoutContext) ?? { setDataDrawerOpen: () => {} };
  const [anchorEl, setAnchorEl] = React.useState(null);
  const shortcuts = React.useContext(LiveShortcutsContext);
  const { inLiveView } = React.useContext(SidebarContext);

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
      setTourSeen();
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
                <MenuItem key='tour' button component='a' onClick={openTour} className={classes.hideOnMobile}>
                  <StyledListItemIcon>
                    <ExploreIcon />
                  </StyledListItemIcon>
                  <StyledListItemText primary='Tour' />
                </MenuItem>
              ),
              (
                <MenuItem key='shortcuts' button component='a' onClick={() => shortcuts['show-help'].handler()}>
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

const TopBarImpl = ({
  classes, children, toggleSidebar, setSidebarOpen,
}) => (
  <AppBar className={classes.container} position='static'>
    <Toolbar>
      <IconButton edge='start' color='inherit' aria-label='menu' sx={{ mr: 2 }} onClick={toggleSidebar}>
        <MenuIcon className={classes.menu} />
      </IconButton>
      <Link to='/'><Logo /></Link>
      <div className={classes.contents}>
        { children }
      </div>
      <ProfileItem classes={classes} setSidebarOpen={setSidebarOpen} />
    </Toolbar>
  </AppBar>
);

export const TopBar = withStyles((theme: Theme) => createStyles({
  container: {
    zIndex: 1300,
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
  avatarSm: {
    backgroundColor: theme.palette.primary.main,
    width: theme.spacing(4),
    height: theme.spacing(4),
    alignItems: 'center',
  },
  centeredListItemText: {
    paddingLeft: theme.spacing(1),
  },
}))(TopBarImpl);
