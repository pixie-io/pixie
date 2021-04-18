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

import {
  DocsIcon, LogoutIcon, SettingsIcon,
  Avatar, ProfileMenuWrapper,
} from '@pixie-labs/components';
import KeyboardIcon from '@material-ui/icons/Keyboard';
// eslint-disable-next-line @typescript-eslint/no-unused-vars
import { DOMAIN_NAME } from 'containers/constants';
import * as React from 'react';
import { Link } from 'react-router-dom';

import IconButton from '@material-ui/core/IconButton';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import MenuItem from '@material-ui/core/MenuItem';
import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import { useContext } from 'react';
import { useUserInfo } from '@pixie-labs/api-react';
import { LiveShortcutsContext } from '../live/shortcuts';

const useStyles = makeStyles((theme: Theme) => ({
  avatarSm: {
    backgroundColor: theme.palette.primary.main,
    width: theme.spacing(4),
    height: theme.spacing(4),
  },
  avatarLg: {
    backgroundColor: theme.palette.primary.main,
    width: theme.spacing(7),
    height: theme.spacing(7),
  },
  listItemText: {
    ...theme.typography.body2,
  },
  listItemHeader: {
    ...theme.typography.subtitle1,
    color: theme.palette.text.primary,
  },
  centeredListItemText: {
    textAlign: 'center',
  },
  expandedProfile: {
    flexDirection: 'column',
  },
}));

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

const ProfileMenu = (props: { className?: string }) => {
  const classes = useStyles();
  const [open, setOpen] = React.useState<boolean>(false);
  const [anchorEl, setAnchorEl] = React.useState(null);
  const shortcuts = useContext(LiveShortcutsContext);

  const openMenu = React.useCallback((event) => {
    setOpen(true);
    setAnchorEl(event.currentTarget);
  }, []);

  const closeMenu = React.useCallback(() => {
    setOpen(false);
    setAnchorEl(null);
  }, []);

  const [user, loading, error] = useUserInfo();

  if (loading || error || !user) {
    return null;
  }

  return (
    <>
      <IconButton onClick={openMenu} className={props.className || ''}>
        <Avatar name={user.name} picture={user.picture} className={classes.avatarSm} />
      </IconButton>

      <ProfileMenuWrapper
        classes={classes}
        open={open}
        onCloseMenu={closeMenu}
        anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
        anchorEl={anchorEl}
        name={user.name}
        email={user.email}
        picture={user.picture}
      >
        <MenuItem key='admin' button component={Link} to='/admin'>
          <StyledListItemIcon>
            <SettingsIcon />
          </StyledListItemIcon>
          <StyledListItemText primary='Admin' />
        </MenuItem>
        <MenuItem key='docs' button component='a' href={`https://docs.${DOMAIN_NAME}`} target='_blank'>
          <StyledListItemIcon>
            <DocsIcon />
          </StyledListItemIcon>
          <StyledListItemText primary='Documentation' />
        </MenuItem>
        <MenuItem key='shortcuts' button component='button' onClick={() => shortcuts['show-help'].handler()}>
          <StyledListItemIcon>
            <KeyboardIcon />
          </StyledListItemIcon>
          <StyledListItemText primary='Keyboard Shortcuts' />
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

export default ProfileMenu;
