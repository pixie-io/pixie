import * as React from 'react';

import Link from '@material-ui/core/Link';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import { VizierQueryError } from '@pixie/api';
import { CONTACT_ENABLED } from 'containers/constants';

const useStyles = makeStyles((theme: Theme) => createStyles({
  errorRow: {
    ...theme.typography.body2,
    fontFamily: '"Roboto Mono", Monospace',
    marginLeft: `-${theme.spacing(3.3)}px`,
    paddingBottom: theme.spacing(0.5),
  },
  link: {
    cursor: 'pointer',
  },
}));

export const VizierErrorDetails = (props: { error: Error }) => {
  const { error } = props;
  const classes = useStyles();
  const { details } = error as VizierQueryError;

  let errorDetails = null;

  if (typeof details === 'string') {
    errorDetails = <div className={classes.errorRow}>{details}</div>;
  } else if (Array.isArray(details)) {
    errorDetails = (
      <>
        {
          details.map((err, i) => <div key={i} className={classes.errorRow}>{err}</div>)
        }
      </>
    );
  } else {
    errorDetails = <div className={classes.errorRow}>{error.message}</div>;
  }
  return (
    <>
      {errorDetails}
      {CONTACT_ENABLED && (
        <div className={classes.errorRow}>
          Need help?&nbsp;
          <Link className={classes.link} id='intercom-trigger'>Chat with us</Link>
          .
        </div>
      )}
    </>
  );
};
