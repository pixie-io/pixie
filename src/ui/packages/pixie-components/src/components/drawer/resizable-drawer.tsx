import * as React from 'react';
import { DraggableCore } from 'react-draggable';
import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core/styles';

import { FixedSizeDrawer, DrawerDirection } from './drawer';

// The amount of time elasped since a user has last resized the drawer.
const RESIZE_WAIT_INTERVAL_MS = 100;

const styles = ({ spacing }: Theme) => createStyles({
  draggableContent: {
    flex: 1,
    minHeight: 0,
    minWidth: 0,
  },
  verticalDragHandle: {
    cursor: 'row-resize',
    width: '100%',
    height: spacing(1),
    zIndex: 1500,
    position: 'absolute',
  },
  horizontalDragHandle: {
    cursor: 'col-resize',
    height: '100%',
    width: spacing(1),
    zIndex: 1500,
    position: 'absolute',
  },
});

interface ResizableDrawerProps extends WithStyles<typeof styles> {
  children?: React.ReactNode; // The contents of the drawer.
  otherContent?: React.ReactNode; // The content that is not in the drawer.
  drawerDirection: DrawerDirection;
  open: boolean;
  initialSize: number;
  overlay: boolean;
}

const ResizableDrawerImpl = ({
  classes, children, otherContent, drawerDirection, open, overlay, initialSize,
}: ResizableDrawerProps) => {
  const [drawerSize, setDrawerSize] = React.useState(initialSize);
  // State responsible for tracking whether the user is actively resizing. This is used for debouncing.
  const [timer, setTimer] = React.useState(null);

  const resize = React.useCallback(({ deltaY, deltaX }) => {
    clearTimeout(timer);
    const newTimer = setTimeout(() => {
      window.dispatchEvent(new Event('resize'));
    }, RESIZE_WAIT_INTERVAL_MS);
    setTimer(newTimer);

    switch (drawerDirection) {
      case 'top':
        setDrawerSize(drawerSize + deltaY);
        break;
      case 'bottom':
        setDrawerSize(drawerSize - deltaY);
        break;
      case 'left':
        setDrawerSize(drawerSize + deltaX);
        break;
      case 'right':
        setDrawerSize(drawerSize - deltaX);
        break;
      default:
        break;
    }
  }, [drawerDirection, drawerSize, timer]);

  let dragHandleStyle = {};
  // If the drawer is in overlay mode, we need additional styling to place the drag
  // handle in the correct place, since we can't rely on the drawer being in the
  // DOM anymore.
  if (overlay) {
    switch (drawerDirection) {
      case 'top':
        dragHandleStyle = { left: 0, marginTop: drawerSize };
        break;
      case 'bottom':
        dragHandleStyle = { left: 0, bottom: 0, marginBottom: drawerSize };
        break;
      case 'left':
        dragHandleStyle = { top: 0, marginLeft: drawerSize };
        break;
      case 'right':
        dragHandleStyle = { top: 0, right: 0, marginRight: drawerSize };
        break;
      default:
        break;
    }
  }

  const dragHandle = (
    <DraggableCore
      onDrag={(_, { deltaX, deltaY }) => {
        resize({
          deltaY,
          deltaX,
        });
      }}
    >
      <div
        style={dragHandleStyle}
        className={drawerDirection === 'top' || drawerDirection === 'bottom'
          ? classes.verticalDragHandle : classes.horizontalDragHandle}
      />
    </DraggableCore>
  );

  let dragFlex = {};
  if (drawerDirection === 'left' || drawerDirection === 'right') {
    dragFlex = { display: 'flex', flexDirection: 'column' };
  }

  const draggableContent = (
    <div className={classes.draggableContent} style={dragFlex}>
      {
        (drawerDirection === 'top' || drawerDirection === 'left') && open
        && dragHandle
      }
      {otherContent}
      {
        (drawerDirection === 'bottom' || drawerDirection === 'right') && open
        && dragHandle
      }
    </div>
  );

  return (
    <>
      <FixedSizeDrawer
        otherContent={draggableContent}
        drawerDirection={drawerDirection}
        open={open}
        overlay={overlay}
        drawerSize={`${drawerSize}px`}
      >
        {children}
      </FixedSizeDrawer>
    </>
  );
};

export const ResizableDrawer = withStyles(styles)(ResizableDrawerImpl);
