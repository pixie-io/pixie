import * as React from 'react';
import Drawer from '@material-ui/core/Drawer';
import { createStyles, WithStyles, withStyles } from '@material-ui/core/styles';

export type DrawerDirection = 'top' | 'bottom' | 'left' | 'right';

const styles = () => createStyles({
  root: {
    display: 'flex',
    flex: 1,
    minHeight: 0,
    minWidth: 0,
    position: 'relative',
  },
  otherContent: {
    flex: 1,
    display: 'flex',
    width: '100%',
  },
  drawerContents: {
    display: 'flex',
    width: '100%',
    height: '100%',
  },
  drawerPaper: {
    position: 'absolute',
  },
  dockedVertical: {
    width: 0,
    flex: 0,
  },
  dockedHorizontal: {
    height: 0,
    flex: 0,
  },
});

interface FixedSizeDrawerProps extends WithStyles<typeof styles> {
  children?: React.ReactNode; // The contents of the drawer.
  otherContent?: React.ReactNode; // The content that is not in the drawer.
  drawerDirection: DrawerDirection;
  drawerSize: string; // A fixed size for the drawer.
  open: boolean;
  overlay: boolean;
}

const FixedSizeDrawerImpl = ({
  classes,
  children,
  otherContent,
  drawerDirection,
  drawerSize,
  open,
  overlay,
}: FixedSizeDrawerProps) => {
  const drawerStyle = drawerDirection === 'top' || drawerDirection === 'bottom'
    ? { height: drawerSize }
    : { width: drawerSize };

  let contentStyle = {};
  if (open && !overlay) {
    // When the drawer is open, the other content should shrink by `drawerSize`.
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
      <Drawer
        anchor={drawerDirection}
        style={drawerStyle}
        variant='persistent'
        open={open}
        classes={{
          paper: classes.drawerPaper,
          docked:
            drawerDirection === 'top' || drawerDirection === 'bottom'
              ? classes.dockedVertical
              : classes.dockedHorizontal,
        }}
      >
        <div className={classes.drawerContents} style={drawerStyle}>
          {children}
        </div>
      </Drawer>
    </div>
  );
};

export const FixedSizeDrawer = withStyles(styles)(FixedSizeDrawerImpl);
