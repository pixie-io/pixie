import * as React from 'react';
import { useLocation } from 'react-router';
import { PixienautBox } from '@pixie-labs/components';
import {
  Button,
  createStyles,
  makeStyles,
  Theme,
  Typography,
} from '@material-ui/core';

const useStyles = makeStyles(({ palette, spacing }: Theme) => createStyles({
  title: {
    color: palette.foreground.two,
  },
  message: {
    color: palette.foreground.one,
    marginTop: spacing(4),
    marginBottom: spacing(4),
  },
  footer: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-around',
    alignItems: 'center',
    paddingTop: spacing(4),
    paddingBottom: spacing(4),
    borderTop: `1px solid ${palette.foreground.grey1}`,
    width: '81%', // Lines up with the border under the octopus graphic and puts space between the buttons
  },
}));

export const RouteNotFound = () => {
  const styles = useStyles();
  const { pathname } = useLocation();
  return (
    <PixienautBox image='octopus'>
      <Typography variant='h1' className={styles.title}>
        404 Not Found
      </Typography>
      <p className={styles.message}>
        {/* eslint-disable-next-line react/jsx-one-expression-per-line */}
        The route &quot;<code>{pathname}</code>&quot; doesn&apos;t exist.
      </p>
      <div className={styles.footer}>
        <Button href='/logout' variant='text'>Log Out</Button>
        <Button href='/live' variant='contained' color='primary'>Live View</Button>
      </div>
    </PixienautBox>
  );
};
