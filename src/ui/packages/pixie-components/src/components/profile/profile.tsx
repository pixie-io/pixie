import * as React from 'react';

import BaseAvatar from '@material-ui/core/Avatar';
import ListItemText from '@material-ui/core/ListItemText';
import Menu from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';

interface AvatarProps {
  name: string;
  picture?: string;
  className?: string;
}

export const Avatar = (props: AvatarProps) => {
  // When the picture is an empty string, the fallback letter-style avatar of alt isn't used.
  // That only happens when the picture field is an invalid link.
  if (!props.picture && props.name.length > 0) {
    return <BaseAvatar className={props.className}>{props.name[0]}</BaseAvatar>;
  }
  return <BaseAvatar src={props.picture} alt={props.name} className={props.className} />;
};

export const ProfileMenuWrapper = ({
  classes, children, anchorOrigin, open, onCloseMenu, anchorEl, name, picture, email,
}) => (
  <Menu
    open={open}
    onClose={onCloseMenu}
    onBlur={onCloseMenu}
    anchorEl={anchorEl}
    getContentAnchorEl={null}
    anchorOrigin={anchorOrigin}
  >
    <MenuItem key='profile' alignItems='center' button={false} className={classes.expandedProfile}>
      <Avatar name={name} picture={picture} className={classes.avatarLg} />
      <ListItemText
        primary={name}
        secondary={email}
        classes={{ primary: classes.listItemHeader, secondary: classes.listItemText }}
        className={classes.centeredListItemText}
      />
    </MenuItem>
    { children }
  </Menu>
);
