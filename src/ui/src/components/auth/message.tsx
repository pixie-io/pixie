import * as React from 'react';
import {
  Box,
  Container, createStyles, fade, Theme, Typography, withStyles, WithStyles,
} from '@material-ui/core';
import Grid from '@material-ui/core/Grid';
import clsx from 'clsx';
import * as pixienautSVG from '../../../assets/images/pixienaut.svg';
import * as authErrorSVG from './auth-error.svg';
import CodeRenderer from '../code-renderer/code-renderer';

const styles = ({ palette, spacing }: Theme) => createStyles({
  root: {
    backgroundColor: fade(palette.foreground.grey3, 0.8),
    paddingLeft: spacing(6),
    paddingRight: spacing(6),
    paddingTop: spacing(10),
    paddingBottom: spacing(10),
    boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px rgba(0, 0, 0, 0.6)`,
    borderRadius: spacing(3),
  },
  centerText: {
    textAlign: 'center',
  },
  centerContent: {
    display: 'flex',
    justifyContent: 'center',
  },
  title: {
    color: palette.foreground.two,
  },
  message: {
    color: palette.foreground.one,
    paddingTop: spacing(2),
  },
  errorDetails: {
    color: palette.foreground.grey4,
  },
});

export interface MessageBoxProps extends WithStyles<typeof styles> {
  error?: boolean;
  title: string;
  message: string;
  errorDetails?: string;
  code?: string;
}

export const MessageBox = withStyles(styles)((props: MessageBoxProps) => {
  const {
    error,
    errorDetails,
    title,
    message,
    code,
    classes,
  } = props;
  return (
    <Box maxWidth={0.9} maxHeight={500} className={classes.root}>
      <Container maxWidth='sm'>
        <Grid container justify='center' direction='column' spacing={5}>
          <Grid item className={classes.centerContent}>
            {error
              ? <img src={authErrorSVG} alt='error' />
              : <img src={pixienautSVG} alt='pixienaut' />}
          </Grid>
          <Grid item className={classes.centerContent}>
            <Typography variant='h4' className={classes.title}>
              {title}
            </Typography>
          </Grid>
          <Grid container justify='center' direction='column' spacing={2}>
            <Grid item className={clsx(classes.centerContent, classes.centerText)}>
              <Typography variant='h6' className={classes.message}>
                {message}
              </Typography>
            </Grid>
            {code
            && (
              <Grid item className={classes.centerContent}>
                <CodeRenderer
                  code={code}
                />
              </Grid>
            )}
            {error && errorDetails
            && (
              <Grid item className={classes.centerContent}>
                <Typography variant='body1' className={classes.errorDetails}>
                  {`Details: ${errorDetails}`}
                </Typography>
              </Grid>
            )}
          </Grid>
        </Grid>
      </Container>
    </Box>
  );
});
