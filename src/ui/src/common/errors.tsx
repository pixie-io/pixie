import * as React from 'react';
import {Status} from 'types/generated/vizier_pb';

import {createStyles, makeStyles, Theme, withStyles} from '@material-ui/core/styles';

export type VizierQueryErrorType = 'script' | 'vis' | 'execution' | 'server';

export class VizierQueryError extends Error {
  constructor(
    public errType: VizierQueryErrorType,
    public details?: string | string[],
    public status?: Status) {
    super(getUserFacingMessage(errType));
  }
}

function getUserFacingMessage(errType: VizierQueryErrorType): string {
  switch (errType) {
    case 'vis':
      return 'Invalid Vis spec';
    case 'script':
      return 'Invalid PXL script';
    case 'execution':
      return 'Failed to execute script';
    case 'server':
      return 'Unexpected error';
    default:
      // Not reached
      return 'Unknown error';
  }
}

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    errorRow: {
      padding: theme.spacing(2),
      borderBottom: `solid 1px ${theme.palette.background.three}`,
    },
  }));

export const VizierErrorDetails = (props: { error: Error }) => {
  const { error } = props;
  const classes = useStyles();
  const details = (error as VizierQueryError).details;

  if (details === 'string') {
    return <div className={classes.errorRow}>{details}</div>;
  }
  if (Array.isArray(details)) {
    return (
      <>
        {
          details.map((err, i) => <div key={i} className={classes.errorRow}>{err}</div>)
        }
      </>
    );
  }
  return <div className={classes.errorRow}>{error.message}</div>;
};
