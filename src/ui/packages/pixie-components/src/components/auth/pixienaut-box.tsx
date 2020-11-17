import * as React from 'react';
import {
  createStyles,
  fade,
  Theme,
  WithStyles,
  withStyles,
} from '@material-ui/core';
import { PixienautBalloonSvg } from './pixienaut-balloon';

const styles = ({ spacing, palette, breakpoints }: Theme) =>
  createStyles({
    root: {
      backgroundColor: fade(palette.foreground.grey3, 0.8),
      paddingLeft: spacing(5),
      paddingRight: spacing(5),
      paddingTop: spacing(0),
      paddingBottom: spacing(1),
      boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px rgba(0, 0, 0, 0.6)`,
      borderRadius: spacing(3),
      minWidth: '370px',
      maxWidth: breakpoints.values.xs,
      maxHeight: '550px',
    },
    splashImageContainer: {
      display: 'flex',
      justifyContent: 'center',
      alignItems: 'flex-start',
      padding: spacing(3),
    },
    pixienautContainer: {
      // The Pixienaut is still kinda close to Earth (this component's content)
      // This + splash container padding + relative Pixienaut position = spacing(4) under its foot
      marginBottom: spacing(-9),
      marginTop: spacing(-2),
    },
    pixienautImage: {
      // The balloons raise the Pixienaut up out of the container
      // (and correcting for unusual image dimensions)
      position: 'relative',
      bottom: spacing(8.5),
      left: spacing(2),
      padding: 0,
    },
    content: {
      display: 'flex',
      flexFlow: 'column nowrap',
      alignItems: 'center',
      justifyContent: 'flex-start',
      width: '100%',
      textAlign: 'center',
    },
  });

export interface PixienautBoxProps extends WithStyles<typeof styles> {
  overrideImage?: React.ReactNode;
  children?: React.ReactNode;
}

export const PixienautBox = withStyles(styles)(
  ({ classes, children, overrideImage }: PixienautBoxProps) => (
    <div className={classes.root}>
      <div className={classes.splashImageContainer}>
        {overrideImage ?? (
          <div className={classes.pixienautContainer}>
            <PixienautBalloonSvg className={classes.pixienautImage} />
          </div>
        )}
      </div>
      <div className={classes.content}>{children}</div>
    </div>
  )
);
