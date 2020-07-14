// This is the primary auth box, which has either the login or signin variant.
import * as React from 'react';
import {
  Box,
  Button,
  Container,
  createStyles,
  fade,
  Grid, Link,
  Theme,
  Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import clsx from 'clsx';
import pixienautBalloonSvg from './pixienaut-balloon.svg';
import { GoogleIcon } from '../icons/google';

const styles = ({ spacing, palette }: Theme) => createStyles({
  root: {
    backgroundColor: fade(palette.foreground.grey3, 0.8),
    paddingLeft: spacing(5),
    paddingRight: spacing(5),
    paddingTop: spacing(0),
    paddingBottom: spacing(1),
    boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px rgba(0, 0, 0, 0.6)`,
    borderRadius: spacing(3),
  },
  flex: {
    display: 'flex',
  },
  title: {
    color: palette.foreground.two,
    paddingBottom: spacing(2),
    marginBottom: spacing(1.25),
  },
  bodyText: {
    margin: 0,
  },
  account: {
    color: palette.foreground.grey4,
  },
  textCenter: {
    textAlign: 'center',
  },
  button: {
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    textTransform: 'capitalize',
  },
  pixienaut: {
    position: 'relative',
    bottom: spacing(7.5),
    left: spacing(2),
    // We need to take the margin out of the bottom because we do relative placement.
    marginBottom: -1 * spacing(5.5),
  },
  gutter: {
    marginTop: spacing(2),
    borderTop: `1px solid ${palette.foreground.grey1}`,
  },
  centerSelf: {
    alignSelf: 'center',
  },
});

export interface AuthBoxProps extends WithStyles<typeof styles> {
  variant: 'login' | 'signup';
  onPrimaryButtonClick?: () => void;
  toggleURL?: string;
}

const textByVariant = {
  signup: {
    title: 'Get Started',
    body: 'Pixie Community is Free Forever.\nNo Credit Card Needed',
    googleButtonText: 'Sign-up with Google',
    buttonCaption: 'Already have an account?',
    buttonText: 'Login',
  },
  login: {
    title: 'Login',
    body: 'Welcome back to Pixie!',
    googleButtonText: 'Login with Google',
    buttonCaption: 'Don\'t have an account yet?',
    buttonText: 'Sign Up',
  },
};

export
const AuthBox = withStyles(styles)((props: AuthBoxProps) => {
  const {
    onPrimaryButtonClick,
    toggleURL,
    variant,
    classes,
  } = props;
  return (
    <Box minWidth={370} maxWidth={0.95} maxHeight={550} className={classes.root}>
      <Container maxWidth='sm'>
        <Grid container direction='column' spacing={2}>
          <Grid item className={clsx(classes.flex, classes.centerSelf)}>
            <img src={pixienautBalloonSvg} alt='pixienaut' className={classes.pixienaut} />
          </Grid>
          <Grid item className={clsx(classes.flex, classes.centerSelf)}>
            <Typography variant='h4' className={clsx(classes.title, classes.textCenter)}>
              {textByVariant[variant].title}
            </Typography>
          </Grid>
          <Grid item className={clsx(classes.flex, classes.title, classes.centerSelf)}>
            <Typography variant='subtitle1' className={clsx(classes.title, classes.textCenter)}>
              <span>
                {textByVariant[variant].body.split('\n').map((s, i) => (
                  <p className={classes.bodyText} key={i}>{s}</p>
                ))}
              </span>
            </Typography>
          </Grid>
          <Grid item className={clsx(classes.flex, classes.centerSelf)}>
            <Button
              variant='contained'
              color='primary'
              className={classes.button}
              startIcon={<GoogleIcon />}
              onClick={() => onPrimaryButtonClick && onPrimaryButtonClick()}
            >
              {textByVariant[variant].googleButtonText}
            </Button>
          </Grid>
          <Grid item className={clsx(classes.flex, classes.gutter)}>
            <Grid container direction='row' justify='center' alignItems='center'>
              <Grid item>
                <Typography variant='subtitle2' className={clsx(classes.account, classes.textCenter)}>
                  {textByVariant[variant].buttonCaption}
                </Typography>
              </Grid>
              <Grid item>
                <Button component={Link} color='secondary' href={toggleURL}>{textByVariant[variant].buttonText}</Button>
              </Grid>
            </Grid>
          </Grid>
        </Grid>
      </Container>
    </Box>
  );
});
