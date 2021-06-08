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

import { WithStyles } from '@material-ui/core';
import BaseAvatar from '@material-ui/core/Avatar';
import ListItemText from '@material-ui/core/ListItemText';
import Menu, { MenuProps } from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';

interface AvatarProps {
  name: string;
  picture?: string;
  className?: string;
}

export const Avatar: React.FC<AvatarProps> = (props) => {
  // When the picture is an empty string, we use a fallback letter-style avatar.
  // If we don't have a name either, this shows a silhouette.
  const name = props.name.trim();
  if (!props.picture && name.length > 0) {
    return <BaseAvatar className={props.className}>{name[0]}</BaseAvatar>;
  }
  return (
    <BaseAvatar
      src={props.picture}
      alt={name}
      className={props.className}
    />
  );
};

interface ProfileMenuWrapperProps extends WithStyles<any>, Pick<MenuProps, 'anchorOrigin'|'open'|'anchorEl'> {
  onCloseMenu: () => void;
  name: string;
  email: string;
  picture?: string;
}

export const ProfileMenuWrapper: React.FC<ProfileMenuWrapperProps> = ({
  classes,
  children,
  anchorOrigin,
  open,
  onCloseMenu,
  anchorEl,
  name,
  picture,
  email,
}) => (
  <Menu
    open={open}
    onClose={onCloseMenu}
    onBlur={onCloseMenu}
    anchorEl={anchorEl}
    anchorOrigin={anchorOrigin}
  >
    <MenuItem
      key='profile'
      alignItems='center'
      button={false}
      className={classes.expandedProfile}
    >
      <Avatar name={name} picture={picture} className={classes.avatarSm} />
      <ListItemText
        primary={name}
        secondary={email}
        classes={{
          primary: classes.listItemHeader,
          secondary: classes.listItemText,
        }}
        className={classes.centeredListItemText}
      />
    </MenuItem>
    {children}
  </Menu>
);
