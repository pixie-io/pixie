import * as React from 'react';
import {
  createStyles, WithStyles, withStyles, Theme,
} from '@material-ui/core/styles';

import SideBar from 'containers/App/sidebar';

const styles = () => createStyles({});

interface NavBarsProps extends WithStyles<typeof styles> {
  appBarContents?: React.ReactNode;
  children?: React.ReactNode;
}

const NavBars = ({
  classes, children,
}: NavBarsProps) => (
  <>
    <SideBar />
  </>
);

export default withStyles(styles)(NavBars);
