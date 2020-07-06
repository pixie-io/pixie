import * as React from 'react';
import Drawer from '@material-ui/core/Drawer';
import {
  createStyles, WithStyles, withStyles,
} from '@material-ui/core/styles';

export type DrawerDirection = 'top' | 'bottom' | 'left' | 'right';

const styles = () => createStyles({
  root: {
    display: 'flex',
    flex: 1,
    minHeight: 0,
    minWidth: 0,
  },
  otherContent: {
    flex: 1,
    display: 'flex',
  },
  drawerContents: {
    display: 'flex',
  },
});

interface FixedSizeDrawerProps extends WithStyles<typeof styles> {
  children?: React.ReactNode; // The contents of the drawer.
  otherContent?: React.ReactNode; // The content that is not in the drawer.
  drawerDirection: DrawerDirection;
  drawerSize: string; // A fixed size for the drawer.
  open: boolean;
}

const FixedSizeDrawer = ({
  classes, children, otherContent, drawerDirection, drawerSize, open,
}: FixedSizeDrawerProps) => {
  const drawerStyle = drawerDirection === 'top' || drawerDirection === 'bottom' ? { height: drawerSize }
    : { width: drawerSize };

  let contentStyle = {};
  if (open) { // When the drawer is open, the other content should shrink by `drawerSize`.
    if (drawerDirection === 'top') {
      contentStyle = { marginTop: drawerSize };
    } else if (drawerDirection === 'bottom') {
      contentStyle = { marginBottom: drawerSize };
    } else if (drawerDirection === 'left') {
      contentStyle = { marginLeft: drawerSize };
    } else {
      contentStyle = { marginRight: drawerSize };
    }
  }

  return (
    <div className={classes.root}>
      <div className={classes.otherContent} style={contentStyle}>
        {otherContent}
      </div>
      <Drawer anchor={drawerDirection} style={drawerStyle} variant='persistent' open={open}>
        <div className={classes.drawerContents} style={drawerStyle}>
          {children}
        </div>
      </Drawer>
    </div>
  );
};

export default withStyles(styles)(FixedSizeDrawer);
