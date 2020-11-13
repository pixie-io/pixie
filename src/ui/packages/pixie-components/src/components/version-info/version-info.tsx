import * as React from 'react';
import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core/styles';

const styles = ({ palette }: Theme) => createStyles({
  root: {
    position: 'absolute',
    top: 0,
    right: 0,
    color: palette.grey['800'],
    fontSize: '12px',
  },
});

interface VersionInfoProps extends WithStyles<typeof styles> {
  cloudVersion: string;
}

const VersionInfoImpl = (props: VersionInfoProps) => {
  const { classes, cloudVersion } = props;
  return (
    <div className={classes.root}>
      {cloudVersion}
    </div>
  );
};

export const VersionInfo = withStyles(styles)(VersionInfoImpl);
