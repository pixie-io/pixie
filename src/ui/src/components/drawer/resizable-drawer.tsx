import * as React from 'react';
import { DraggableCore } from 'react-draggable';
import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core/styles';

import { FixedSizeDrawer, DrawerDirection } from './drawer';

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

const ResizableDrawer = ({
  classes, children, otherContent, drawerDirection, open, overlay, initialSize,
}: ResizableDrawerProps) => {
  const [drawerSize, setDrawerSize] = React.useState(initialSize);

  const resize = React.useCallback(({ deltaY, deltaX }) => {
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
  }, [drawerDirection, drawerSize]);

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
      onDrag={(event, { deltaX, deltaY }) => {
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

  const draggableContent = (
    <div className={classes.draggableContent}>
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

export default withStyles(styles)(ResizableDrawer);
