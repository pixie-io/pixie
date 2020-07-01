import * as React from 'react';
import { PIXIE_CLOUD_VERSION } from 'utils/env';
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
  cloudVersion?: string;
}

const VersionInfo = (props: VersionInfoProps) => {
  const { classes, cloudVersion } = props;
  const version = cloudVersion || PIXIE_CLOUD_VERSION;
  return (
    <div className={classes.root}>
      {version}
    </div>
  );
};

export default withStyles(styles)(VersionInfo);
