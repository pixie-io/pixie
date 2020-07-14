import * as React from 'react';
import {
  Button, createStyles, Grid, Theme, Typography, withStyles, WithStyles,
} from '@material-ui/core';
import clsx from 'clsx';

const styles = ({ spacing, breakpoints }: Theme) => createStyles({
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
});

type FooterProps = WithStyles<typeof styles>;

export const Footer = withStyles(styles)(({ classes }: FooterProps) => (
  <Grid container direction='row' alignContent='flex-end'>
    <Grid item xs={false} sm={6} className={classes.gridItem}>
      <Typography variant='subtitle2' className={clsx(classes.paddingRight)}>
        <Button disabled size='small'>Terms & Conditions</Button>
      </Typography>
      <Typography variant='subtitle2'>
        <Button disabled size='small'>Privacy Policy</Button>
      </Typography>
    </Grid>
    <Grid
      item
      xs={false}
      sm={6}
      className={clsx(classes.copyright, classes.gridItem)}
    >
      <Typography variant='subtitle2'>
        &copy; 2020,  Pixie Labs Inc.
      </Typography>
    </Grid>
  </Grid>
));
