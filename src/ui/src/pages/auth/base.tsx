// This contains the base template all auth pages. We really only allow inserting the
// middle part of the page which has either a dialog, or marcom information.
import * as React from 'react';
import {
  Container,
  createStyles, Grid, Theme, withStyles, WithStyles,
} from '@material-ui/core';
import clsx from 'clsx';
import * as StarsPNG from './stars.png';
import * as pixieLogo from '../../../assets/images/pixie-logo.svg';
import { Footer } from '../../components/auth/footer';

const styles = ({ spacing, breakpoints }: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    minWidth: '400px',
    backgroundImage: `url(${StarsPNG})`,
    display: 'flex',
  },
  flex: {
    display: 'flex',
  },
  flexGrow: {
    flexGrow: 1,
  },
  xsCenter: {
    [breakpoints.down('xs')]: {
      justifyContent: 'center',
    },
  },
  logo: {
    margin: spacing(4),
    height: spacing(4.5),
  },
});

export interface BasePageProps extends WithStyles<typeof styles> {
  children?: React.ReactNode;
}

export const BasePage = withStyles(styles)(({ children, classes }: BasePageProps) => (
  <>
    <Grid
      container
      direction='column'
      spacing={2}
      className={classes.root}
    >
      <Grid item className={clsx(classes.flex, classes.xsCenter)}>
        <img src={pixieLogo} alt='Pixie Logo' className={classes.logo} />
      </Grid>
      <Grid
        item
        className={clsx(classes.flex, classes.flexGrow)}
      >
        <Container maxWidth='xl' className={classes.flex}>
          {children}
        </Container>
      </Grid>
      <Grid
        item
        className={clsx(classes.flex)}
      >
        <Footer />
      </Grid>
    </Grid>
  </>
));
