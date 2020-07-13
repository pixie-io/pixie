import * as React from 'react';
import {
  Box,
  Container, createStyles, fade, Theme, Typography, withStyles, WithStyles,
} from '@material-ui/core';
import Grid from '@material-ui/core/Grid';
import clsx from 'clsx';
import * as pixienautSVG from '../../../assets/images/pixienaut.svg';

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
  flex: {
    display: 'flex',
  },
  textCenter: {
    textAlign: 'center',
  },
  title: {
    color: palette.foreground.two,
  },
  message: {},
});

export interface MessageBoxProps extends WithStyles<typeof styles> {
  title: string;
  message: string;
}

export const MessageBox = withStyles(styles)(({ title, message, classes }: MessageBoxProps) => (
  <Box maxWidth={0.9} maxHeight={500} className={classes.root}>
    <Container maxWidth='sm'>
      <Grid container direction='column' spacing={5}>
        <Grid item justify='center' className={classes.flex}>
          <img src={pixienautSVG} alt='pixienaut' />
        </Grid>
        <Grid item justify='center' className={classes.flex}>
          <Typography variant='h4' className={clsx(classes.title, classes.textCenter)}>
            {title}
          </Typography>
        </Grid>
        <Grid item className={classes.flex}>
          <Typography variant='h6' className={clsx(classes.message, classes.textCenter)}>
            {message}
          </Typography>
        </Grid>
      </Grid>
    </Container>
  </Box>
));
