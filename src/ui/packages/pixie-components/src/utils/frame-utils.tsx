import * as React from 'react';

import {
  Box,
  createStyles,
  Theme,
  WithStyles,
  withStyles,
} from '@material-ui/core';

const styles = ({ palette, spacing }: Theme) =>
  createStyles({
    root: {
      backgroundColor: palette.background.default,
      padding: spacing(12),
    },
  });

export interface FrameElementProps extends WithStyles<typeof styles> {
  width?: number;
  children?: JSX.Element;
}

export const FrameElement = withStyles(styles)(
  ({ width, children, classes }: FrameElementProps) => (
    <Box width={width} className={classes.root}>
      {children}
    </Box>
  )
);
