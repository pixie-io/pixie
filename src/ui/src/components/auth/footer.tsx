import * as React from 'react';
import {
  Button, createStyles, Grid, Theme, Typography, withStyles, WithStyles,
} from '@material-ui/core';
import clsx from 'clsx';

const styles = ({ spacing, breakpoints, palette }: Theme) => createStyles({
  root: {
    display: 'flex',
  },
  paddingRight: {
    paddingRight: spacing(6),
  },
  gridItem: {
    display: 'flex',
    padding: '10px',
    flexGrow: 1,
    alignItems: 'center',
    [breakpoints.only('xs')]: {
      justifyContent: 'center',
    },
  },
  copyright: {
    justifyContent: 'flex-end',
    [breakpoints.only('xs')]: {
      justifyContent: 'center',
    },
  },
  footerLinks: {
    textDecoration: 'none',
    color: palette.foreground.three,
  },
  footerText: {
    color: palette.foreground.three,
  },
});

type FooterProps = WithStyles<typeof styles>;

export const Footer = withStyles(styles)(({ classes }: FooterProps) => (
  <Grid container direction='row' alignContent='flex-end'>
    <Grid item xs={false} sm={6} className={classes.gridItem}>
      <a
        href='https://pixielabs.ai/terms/'
        className={clsx(classes.paddingRight, classes.footerLinks, classes.footerText)}
      >
        Terms & Conditions
      </a>
      <a href='https://pixielabs.ai/privacy' className={clsx(classes.footerLinks, classes.footerText)}>
        Privacy Policy
      </a>
    </Grid>
    <Grid
      item
      xs={false}
      sm={6}
      className={clsx(classes.copyright, classes.gridItem)}
    >
      <Typography variant='subtitle2' className={classes.footerText}>
        &copy; 2020,  Pixie Labs Inc.
      </Typography>
    </Grid>
  </Grid>
));
