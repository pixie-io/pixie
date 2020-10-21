// This contains the base template all auth pages. We really only allow inserting the
// middle part of the page which has either a dialog, or marcom information.
import * as React from 'react';
import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core';
import { Footer } from 'components/auth/footer';
import * as pixieLogo from '../../../assets/images/pixie-logo.svg';
import * as StarsPNG from './stars.png';

const styles = ({ spacing, breakpoints }: Theme) => createStyles({
  root: {
    minHeight: breakpoints.values.xs,
    height: '100vh',
    minWidth: '400px',
    width: '100vw',
    overflow: 'auto',
    backgroundImage: `url(${StarsPNG})`,
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'space-between',
    alignItems: 'stretch',
  },
  content: {
    flex: '1 0 auto',
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
  },
  logo: {
    flex: '0 0 auto',
    paddingLeft: spacing(4),
    paddingTop: spacing(4),
    paddingRight: spacing(4),
    paddingBottom: spacing(1.0),
    // Includes vertical padding. spacing(4.5) is given to the image within.
    // Combined with footer height, this perfectly (vertically) centers the box containing the Pixienaut (sans balloons)
    height: spacing(9.5),
    width: '100%',
    [breakpoints.down('sm')]: {
      textAlign: 'center',
      paddingBottom: spacing(11.5), // Clear the balloons that break out of their container
      height: spacing(20), // Account for said clearance
    },
    '& > img': {
      height: '100%',
      width: 'auto',
    },
  },
});

export interface BasePageProps extends WithStyles<typeof styles> {
  children?: React.ReactNode;
}

export const BasePage = withStyles(styles)(({ children, classes }: BasePageProps) => (
  <div className={classes.root}>
    <div className={classes.logo}>
      <img src={pixieLogo} alt='Pixie Logo' />
    </div>
    <div className={classes.content}>
      {children}
    </div>
    <Footer />
  </div>
));
