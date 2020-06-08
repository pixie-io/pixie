import {AdminTooltip, StyledTableCell, StyledTableHeaderCell,
        StyledLeftTableCell, StyledRightTableCell} from './utils';
import { useQuery } from '@apollo/react-hooks';
import Table from '@material-ui/core/Table';
import IconButton from '@material-ui/core/IconButton';
import Input from '@material-ui/core/Input';
import InputAdornment from '@material-ui/core/InputAdornment';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import Visibility from '@material-ui/icons/Visibility';
import VisibilityOff from '@material-ui/icons/VisibilityOff';
import gql from 'graphql-tag';
import { distanceInWords } from 'date-fns';
import * as React from 'react';

const LIST_DEPLOYMENT_KEYS = gql`
{
  deploymentKeys {
    id
    key
    desc
    createdAtMs
  }
}`;

interface DeploymentKeyDisplay {
  id: string;
  idShort: string;
  createdAt: string;
  key: string;
  desc: string;
}

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    deploymentKeyValue: {
      fontWeight: theme.typography.fontWeightLight,
      fontSize: '14px',
      color: '#748790',
      backgroundColor: theme.palette.foreground.grey3,
      borderWidth: 8,
      borderColor: theme.palette.background.default,
    },
  });
});

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

export const DeploymentKeyRow = ({deploymentKey}) => {
  const classes = useStyles();
  const [showKey, setShowKey] = React.useState(false);

  return (
    <TableRow key={deploymentKey.id}>
      <AdminTooltip title={deploymentKey.id}>
        <StyledLeftTableCell>{deploymentKey.idShort}</StyledLeftTableCell>
      </AdminTooltip>
      <StyledTableCell>{deploymentKey.createdAt}</StyledTableCell>
      <StyledTableCell>{deploymentKey.desc}</StyledTableCell>
      <StyledRightTableCell>
        <Input
          className={classes.deploymentKeyValue}
          id='deployment-key'
          fullWidth={true}
          readOnly={true}
          disableUnderline={true}
          type={showKey ? 'text' : 'password'}
          value={deploymentKey.key}
          endAdornment={
            <InputAdornment position='end'>
              <IconButton
                size='small'
                aria-label='toggle key visibility'
                onClick={() => setShowKey(!showKey)}
                edge='end'
              >
                {showKey ? <Visibility/> : <VisibilityOff/>}
              </IconButton>
            </InputAdornment>
          }
        />
      </StyledRightTableCell>
    </TableRow>
  );
}

export const DeploymentKeysTable = () => {
  const { loading, error, data } = useQuery(LIST_DEPLOYMENT_KEYS, { pollInterval: 2500 });
  if (loading || error || !data.deploymentKeys) {
    return null;
  }
  const deploymentKeys = data.deploymentKeys.map((key) => formatDeploymentKey(key));
  return (
    <>
      <Table>
        <TableHead>
          <TableRow>
            <StyledTableHeaderCell>ID</StyledTableHeaderCell>
            <StyledTableHeaderCell>Created</StyledTableHeaderCell>
            <StyledTableHeaderCell>Description</StyledTableHeaderCell>
            <StyledTableHeaderCell>Value</StyledTableHeaderCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {deploymentKeys.map((deploymentKey: DeploymentKeyDisplay) => (
            <DeploymentKeyRow key={deploymentKey.id} deploymentKey={deploymentKey}/>
          ))}
        </TableBody>
      </Table>
    </>
  );
}
