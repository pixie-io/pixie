import UnknownIcon from '@material-ui/icons/Brightness1';
import HealthyIcon from '@material-ui/icons/CheckCircle';
import UnhealthyIcon from '@material-ui/icons/Error';
import { createStyles, makeStyles, Theme, withStyles } from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import TableCell from '@material-ui/core/TableCell';
import Tooltip from '@material-ui/core/Tooltip';
import * as React from 'react';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    unhealthy: {
      color: theme.palette.error.main,
    },
    healthy: {
      color: theme.palette.success.main,
    },
    unknown: {
      color: theme.palette.foreground.grey1,
    },
  });
});

const tooltipStyles = makeStyles(() => {
  return createStyles({
    tooltip: {
      margin: 0,
    },
  });
});

const SEC_MS = 1000;
const MIN_MS = 60 * SEC_MS;
const HOURS_MS = 60 * MIN_MS;

export function convertHeartbeatMS(lastHeartbeatMs: number): string {
  const result = [];
  let time = lastHeartbeatMs;
  const hours = Math.floor(time / (HOURS_MS));
  if (hours > 0) {
    result.push(`${hours} hours`);
    time = time % HOURS_MS;
  }
  const minutes = Math.floor(time / MIN_MS);
  if (minutes > 0) {
    result.push(`${minutes} min`);
    time = time % MIN_MS;
  }
  const seconds = Math.floor(time / SEC_MS);
  result.push(`${seconds} sec`);
  return `${result.join(' ')} ago`;
}

export type VizierStatusGroup = 'healthy' | 'unhealthy' | 'unknown';

export const StatusCell = ({statusGroup}) => {
  const classes = useStyles();
  switch (statusGroup) {
    case 'healthy':
      return (<HealthyIcon fontSize='small' className={classes.healthy}/>);
    case 'unhealthy':
      return (<UnhealthyIcon fontSize='small' className={classes.unhealthy}/>);
    default:
      return (<UnknownIcon fontSize='small' className={classes.unknown}/>);
  }
}

export const AdminTooltip = ({children, title}) => {
  const classes = tooltipStyles();
  return (
    <Tooltip title={title} placement='bottom' interactive classes={classes}>
    {children}
    </Tooltip>
  );
}

export const StyledTabs = withStyles(() =>
  createStyles({
    root: {
      flex: 1,
    },
    indicator: {
      backgroundColor: '#12D6D6',
      height: '4px',
    },
  }),
)(Tabs);

export const StyledTab = withStyles((theme: Theme) =>
  createStyles({
    root: {
      fontSize: '16px',
      fontWeight: theme.typography.fontWeightLight,
      textTransform: 'none',
      '&:focus': {
        color: theme.palette.foreground.two,
      },
    },
  }),
)(Tab);

export const StyledTableHeaderCell = withStyles((theme: Theme) =>
  createStyles({
    root: {
      fontWeight: theme.typography.fontWeightLight,
      fontSize: '14px',
      borderBottom: 'none',
    },
  }),
)(TableCell);

export const StyledTableCell = withStyles((theme: Theme) =>
  createStyles({
    root: {
      fontWeight: theme.typography.fontWeightLight,
      fontSize: '14px',
      color: '#748790',
      backgroundColor: theme.palette.foreground.grey3,
      borderWidth: theme.spacing(1),
      borderColor: theme.palette.background.default,
    },
  }),
)(TableCell);

export const StyledLeftTableCell = withStyles(() =>
  createStyles({
    root: {
      borderRadius: '10px 0px 0px 10px',
    },
  }),
)(StyledTableCell);

export const StyledRightTableCell = withStyles(() =>
  createStyles({
    root: {
      borderRadius: '0px 10px 10px 0px',
    },
  }),
)(StyledTableCell);

export const StyledHiddenInputCell = withStyles((theme: Theme) =>
  createStyles({
    root: {
      fontWeight: theme.typography.fontWeightLight,
      fontSize: '14px',
      color: '#748790',
      backgroundColor: theme.palette.foreground.grey3,
      borderWidth: 8,
      borderColor: theme.palette.background.default,
    },
  }),
)(TableCell);
