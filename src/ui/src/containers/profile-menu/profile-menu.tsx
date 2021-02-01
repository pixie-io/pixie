import {
  DocsIcon, LogoutIcon, SettingsIcon,
  Avatar, ProfileMenuWrapper,
} from 'pixie-components';
import KeyboardIcon from '@material-ui/icons/Keyboard';
// eslint-disable-next-line @typescript-eslint/no-unused-vars
import { DOMAIN_NAME } from 'containers/constants';
import * as React from 'react';
import { Link } from 'react-router-dom';

import { useQuery } from '@apollo/client';
import IconButton from '@material-ui/core/IconButton';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import MenuItem from '@material-ui/core/MenuItem';
import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import { useContext } from 'react';
import { USER_QUERIES } from 'pixie-api';
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

  const { loading, error, data } = useQuery(USER_QUERIES.GET_USER_INFO, { fetchPolicy: 'network-only' });

  if (loading || error || !data.user) {
    return null;
  }
  const { user } = data;
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
