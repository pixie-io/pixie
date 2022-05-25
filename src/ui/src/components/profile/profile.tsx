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

import {
  Avatar as BaseAvatar,
  ListItem,
  ListItemText,
  Menu,
  MenuProps,
} from '@mui/material';

import { WithChildren } from 'app/utils/react-boilerplate';

interface AvatarProps {
  name: string;
  picture?: string;
  className?: string;
}

export const Avatar = React.memo<AvatarProps>((props) => {
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
});
Avatar.displayName = 'Avatar';

interface ProfileMenuWrapperProps extends WithChildren<Pick<MenuProps, 'anchorOrigin' | 'open' | 'anchorEl'>> {
  onCloseMenu: MenuProps['onClose'] & MenuProps['onBlur'];
  name: string;
  email: string;
  domainName: string;
  picture?: string;
  classes: Partial<Record<
  'managedDomainBanner'
  | 'expandedProfile'
  | 'avatarSm'
  | 'listItemHeader'
  | 'listItemText'
  | 'centeredListItemText',
  string>>;
}

export const ProfileMenuWrapper = React.memo<ProfileMenuWrapperProps>(({
  classes,
  children,
  anchorOrigin,
  open,
  onCloseMenu,
  anchorEl,
  name,
  picture,
  email,
  domainName,
}) => (
  <Menu
    open={open}
    onClose={onCloseMenu}
    onBlur={onCloseMenu}
    anchorEl={anchorEl}
    anchorOrigin={anchorOrigin}
  >
    {domainName && (
      <ListItem key='managedDomain' className={classes.managedDomainBanner}>
        <span>
          {'This account is managed by '}
          <strong>{domainName}</strong>
          {'.'}
        </span>
      </ListItem>
    )}
    <ListItem
      key='profile'
      className={classes.expandedProfile}
    >
      <Avatar name={name} picture={picture} className={classes.avatarSm} />
      <ListItemText
        primary={name}
        secondary={email}
        classes={React.useMemo(() => ({
          primary: classes.listItemHeader,
          secondary: classes.listItemText,
        }), [classes])}
        className={classes.centeredListItemText}
      />
    </ListItem>
    {children}
  </Menu>
));
ProfileMenuWrapper.displayName = 'ProfileMenuWrapper';
