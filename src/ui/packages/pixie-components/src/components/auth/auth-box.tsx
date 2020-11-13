// This is the primary auth box, which has either the login or signin variant.
import * as React from 'react';
import {
  Button,
  createStyles,
  Link,
  Theme,
  Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { PixienautBox } from './pixienaut-box';
import { GoogleIcon } from '../icons/google';

const styles = ({ spacing, palette }: Theme) => createStyles({
  bodyText: {
    margin: 0,
  },
  account: {
    color: palette.foreground.grey4,
    textAlign: 'center',
  },
  button: {
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    textTransform: 'capitalize',
  },
  title: {
    color: palette.foreground.two,
    paddingTop: spacing(1),
    paddingBottom: spacing(3),
    marginBottom: spacing(1.25),
  },
  subtitle: {
    color: palette.foreground.two,
    paddingTop: spacing(1.25),
    paddingBottom: spacing(5.25),
    marginBottom: spacing(1.25),
    textAlign: 'center',
  },
  gutter: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'center',
    alignItems: 'center',
    marginTop: spacing(3),
    paddingTop: spacing(1),
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
    body: 'Pixie Community is Free Forever.\nNo Credit Card Needed.',
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

export const AuthBox = withStyles(styles)((props: AuthBoxProps) => {
  const {
    onPrimaryButtonClick,
    toggleURL,
    variant,
    classes,
  } = props;
  return (
    <PixienautBox>
      <Typography variant='h1' className={classes.title}>
        {textByVariant[variant].title}
      </Typography>
      <Typography variant='subtitle1' className={classes.subtitle}>
        <span>
          {textByVariant[variant].body.split('\n').map((s, i) => (
            <p className={classes.bodyText} key={i}>{s}</p>
          ))}
        </span>
      </Typography>
      <Button
        variant='contained'
        color='primary'
        className={classes.button}
        startIcon={<GoogleIcon />}
        onClick={() => onPrimaryButtonClick && onPrimaryButtonClick()}
      >
        {textByVariant[variant].googleButtonText}
      </Button>
      <div className={classes.gutter}>
        <Typography variant='subtitle2' className={classes.account}>
          {textByVariant[variant].buttonCaption}
        </Typography>
        <Button component={Link} color='primary' href={toggleURL}>{textByVariant[variant].buttonText}</Button>
      </div>
    </PixienautBox>
  );
});
