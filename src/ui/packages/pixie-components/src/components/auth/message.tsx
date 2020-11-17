import * as React from 'react';
import {
  createStyles,
  fade,
  Theme,
  Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { AuthErrorSvg } from './auth-error';
import { PixienautBox } from './pixienaut-box';
import { CodeRenderer } from 'components/code-renderer/code-renderer';

const styles = ({ palette, spacing }: Theme) =>
  createStyles({
    root: {
      backgroundColor: fade(palette.foreground.grey3, 0.8),
      paddingLeft: spacing(6),
      paddingRight: spacing(6),
      paddingTop: spacing(10),
      paddingBottom: spacing(10),
      boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px rgba(0, 0, 0, 0.6)`,
      borderRadius: spacing(3),
    },
    title: {
      color: palette.foreground.two,
    },
    message: {
      color: palette.foreground.one,
      marginTop: spacing(5.5),
      marginBottom: spacing(4),
    },
    errorDetails: {
      color: palette.foreground.grey4,
      marginTop: spacing(4),
    },
    extraPadding: {
      height: spacing(6),
    },
  });

export interface AuthMessageBoxProps extends WithStyles<typeof styles> {
  error?: boolean;
  title: string;
  message: string;
  errorDetails?: string;
  code?: string;
}

export const AuthMessageBox = withStyles(styles)(
  (props: AuthMessageBoxProps) => {
    const { error, errorDetails, title, message, code, classes } = props;
    const errorImage = <AuthErrorSvg />;
    return (
      <PixienautBox overrideImage={error ? errorImage : undefined}>
        <Typography variant='h1' className={classes.title}>
          {title}
        </Typography>
        <Typography variant='h6' className={classes.message}>
          {message}
        </Typography>
        {code && <CodeRenderer code={code} />}
        {error && errorDetails && (
          <Typography variant='body1' className={classes.errorDetails}>
            {`Details: ${errorDetails}`}
          </Typography>
        )}
        <div className={classes.extraPadding} />
      </PixienautBox>
    );
  }
);
