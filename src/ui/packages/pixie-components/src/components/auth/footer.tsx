import * as React from 'react';
import {
  createStyles, Theme, Typography, withStyles, WithStyles,
} from '@material-ui/core';

const styles = ({ spacing, breakpoints, palette }: Theme) => createStyles({
  root: {
    display: 'flex',
    flexFlow: 'row wrap',
    justifyContent: 'space-between',
    alignItems: 'flex-end',
    width: '100%',
    margin: 0,
    marginTop: spacing(3),
    [breakpoints.only('xs')]: {
      justifyContent: 'center',
    },

    '& > *': {
      display: 'flex',
      flexFlow: 'row wrap',
      paddingBottom: spacing(3),
      [breakpoints.only('xs')]: {
        justifyContent: 'center',
      },
    },
  },
  left: {
    justifyContent: 'flex-start',
  },
  right: {
    justifyContent: 'flex-end',
  },
  text: {
    padding: `0 ${spacing(3)}px`,
    color: palette.foreground.three,
    textDecoration: 'none',
  },
});

type AuthFooterProps = WithStyles<typeof styles>;

export const AuthFooter = withStyles(styles)(({ classes }: AuthFooterProps) => (
  <div className={classes.root}>
    <div className={classes.left}>
      <a
        href='https://pixielabs.ai/terms/'
        className={classes.text}
      >
        Terms & Conditions
      </a>
      <a href='https://pixielabs.ai/privacy' className={classes.text}>
        Privacy Policy
      </a>
    </div>
    <div className={classes.right}>
      <Typography variant='subtitle2' className={classes.text}>
        &copy; 2020,  Pixie Labs Inc.
      </Typography>
    </div>
  </div>
));
