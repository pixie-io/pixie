import { useMutation, useQuery } from '@apollo/react-hooks';
import Table from '@material-ui/core/Table';
import IconButton from '@material-ui/core/IconButton';
import Input from '@material-ui/core/Input';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import Menu from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';
import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import Actions from '@material-ui/icons/MoreHoriz';
import Copy from '@material-ui/icons/FileCopy';
import Delete from '@material-ui/icons/DeleteForever';
import Visibility from '@material-ui/icons/Visibility';
import VisibilityOff from '@material-ui/icons/VisibilityOff';
import gql from 'graphql-tag';
import { distanceInWords } from 'date-fns';
import * as React from 'react';
import {
  AdminTooltip, StyledTableCell, StyledTableHeaderCell,
  StyledLeftTableCell, StyledRightTableCell,
} from './utils';

const LIST_DEPLOYMENT_KEYS = gql`
{
  deploymentKeys {
    id
    key
    desc
    createdAtMs
  }
}`;

const DELETE_DEPLOY_KEY = gql`
  mutation deleteKey($id: ID!) {
    DeleteDeploymentKey(id: $id)
  }
`;

export const CREATE_DEPLOYMENT_KEY = gql`
  mutation {
    CreateDeploymentKey {
      id
    }
  }
`;

interface DeploymentKeyDisplay {
  id: string;
  idShort: string;
  createdAt: string;
  key: string;
  desc: string;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  deploymentKeyValue: {
    padding: 0,
    fontWeight: theme.typography.fontWeightLight,
    fontSize: '14px',
    color: theme.palette.foreground.one,
    backgroundColor: theme.palette.foreground.grey3,
    borderWidth: 8,
    borderColor: theme.palette.background.default,
  },
  actionsButton: {
    padding: 0,
  },
  copyBtn: {
    minWidth: '30px',
  },
  error: {
    padding: theme.spacing(1),
  },
}));

export const StyledMenu = withStyles((theme: Theme) => createStyles({
  paper: {
    backgroundColor: theme.palette.foreground.grey3,
    borderWidth: 8,
    borderColor: theme.palette.background.default,
  },
}))(Menu);

const StyledListItemIcon = withStyles(() => createStyles({
  root: {
    minWidth: 30,
    marginRight: 5,
  },
}))(ListItemIcon);

const StyledListItemText = withStyles((theme: Theme) => createStyles({
  primary: {
    fontWeight: theme.typography.fontWeightLight,
    fontSize: '14px',
    color: theme.palette.foreground.one,
  },
}))(ListItemText);

export function formatDeploymentKey(depKey): DeploymentKeyDisplay {
  const now = new Date();
  return {
    id: depKey.id,
    idShort: depKey.id.split('-').pop(),
    createdAt: `${distanceInWords(new Date(depKey.createdAtMs), now, { addSuffix: false })} ago`,
    key: depKey.key,
    desc: depKey.desc,
  };
}

export const DeploymentKeyRow = ({ deploymentKey }) => {
  const classes = useStyles();
  const [showKey, setShowKey] = React.useState(false);

  const [open, setOpen] = React.useState<boolean>(false);
  const [anchorEl, setAnchorEl] = React.useState(null);

  const [deleteDeployKey] = useMutation(DELETE_DEPLOY_KEY);

  const openMenu = React.useCallback((event) => {
    setOpen(true);
    setAnchorEl(event.currentTarget);
  }, []);

  const closeMenu = React.useCallback(() => {
    setOpen(false);
    setAnchorEl(null);
  }, []);

  return (
    <TableRow key={deploymentKey.id}>
      <AdminTooltip title={deploymentKey.id}>
        <StyledLeftTableCell>{deploymentKey.idShort}</StyledLeftTableCell>
      </AdminTooltip>
      <StyledTableCell>{deploymentKey.createdAt}</StyledTableCell>
      <StyledTableCell>{deploymentKey.desc}</StyledTableCell>
      <StyledTableCell>
        <Input
          className={classes.deploymentKeyValue}
          id='deployment-key'
          fullWidth
          readOnly
          disableUnderline
          type={showKey ? 'text' : 'password'}
          value={deploymentKey.key}
        />
      </StyledTableCell>
      <StyledRightTableCell>
        <IconButton
          size='small'
          classes={{ sizeSmall: classes.actionsButton }}
          onClick={openMenu}
        >
          <Actions />
        </IconButton>
        <StyledMenu
          open={open}
          onClose={closeMenu}
          anchorEl={anchorEl}
          getContentAnchorEl={null}
          anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
        >
          <MenuItem key='show' alignItems='center' onClick={() => setShowKey(!showKey)}>
            <StyledListItemIcon>
              {showKey ? <Visibility /> : <VisibilityOff />}
            </StyledListItemIcon>
            <StyledListItemText primary={showKey ? 'Hide value' : 'Show value'} />
          </MenuItem>
          <MenuItem
            key='copy'
            alignItems='center'
            onClick={() => navigator.clipboard.writeText(deploymentKey.key)}
          >
            <StyledListItemIcon className={classes.copyBtn}>
              <Copy />
            </StyledListItemIcon>
            <StyledListItemText primary='Copy value' />
          </MenuItem>
          <MenuItem
            key='delete'
            alignItems='center'
            onClick={() => deleteDeployKey({ variables: { id: deploymentKey.id } })}
          >
            <StyledListItemIcon className={classes.copyBtn}>
              <Delete />
            </StyledListItemIcon>
            <StyledListItemText primary='Delete' />
          </MenuItem>
        </StyledMenu>
      </StyledRightTableCell>
    </TableRow>
  );
};

export const DeploymentKeysTable = () => {
  const classes = useStyles();
  const { loading, error, data } = useQuery(LIST_DEPLOYMENT_KEYS, { pollInterval: 2000 });
  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }

  const deploymentKeys = (data?.deploymentKeys || []).map((key) => formatDeploymentKey(key));
  return (
    <>
      <Table>
        <TableHead>
          <TableRow>
            <StyledTableHeaderCell>ID</StyledTableHeaderCell>
            <StyledTableHeaderCell>Created</StyledTableHeaderCell>
            <StyledTableHeaderCell>Description</StyledTableHeaderCell>
            <StyledTableHeaderCell>Value</StyledTableHeaderCell>
            <StyledTableHeaderCell>Actions</StyledTableHeaderCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {deploymentKeys.map((deploymentKey: DeploymentKeyDisplay) => (
            <DeploymentKeyRow key={deploymentKey.id} deploymentKey={deploymentKey} />
          ))}
        </TableBody>
      </Table>
    </>
  );
};
