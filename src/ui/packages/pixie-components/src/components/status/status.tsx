import UnknownIcon from '@material-ui/icons/Brightness1';
import HealthyIcon from '@material-ui/icons/CheckCircle';
import UnhealthyIcon from '@material-ui/icons/Error';
import PendingIcon from '@material-ui/icons/WatchLater';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import * as React from 'react';

export type StatusGroup = 'healthy' | 'unhealthy' | 'pending' | 'unknown';

const useStyles = makeStyles((theme: Theme) => createStyles({
  unhealthy: {
    color: theme.palette.error.main,
  },
  healthy: {
    color: theme.palette.success.main,
  },
  pending: {
    color: theme.palette.warning.main,
  },
  unknown: {
    color: theme.palette.foreground.grey1,
  },
}));

export const StatusCell = ({ statusGroup }: { statusGroup: StatusGroup }) => {
  const classes = useStyles();
  switch (statusGroup) {
    case 'healthy':
      return (<HealthyIcon fontSize='small' className={classes.healthy} />);
    case 'unhealthy':
      return (<UnhealthyIcon fontSize='small' className={classes.unhealthy} />);
    case 'pending':
      return (<PendingIcon fontSize='small' className={classes.pending} />);
    default:
      return (<UnknownIcon fontSize='small' className={classes.unknown} />);
  }
};
