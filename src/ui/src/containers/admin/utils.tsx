import UnknownIcon from '@material-ui/icons/Brightness1';
import HealthyIcon from '@material-ui/icons/CheckCircle';
import UnhealthyIcon from '@material-ui/icons/Error';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
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
