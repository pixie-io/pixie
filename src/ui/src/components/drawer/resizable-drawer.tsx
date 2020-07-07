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
}

const ResizableDrawer = ({
  classes, children, otherContent, drawerDirection, open, initialSize,
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

  const dragHandle = (
    <DraggableCore
      onDrag={(event, { deltaX, deltaY }) => {
        resize({
          deltaY,
          deltaX,
        });
      }}
    >
      <div className={drawerDirection === 'top' || drawerDirection === 'bottom'
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
        drawerSize={`${drawerSize}px`}
      >
        {children}
      </FixedSizeDrawer>
    </>
  );
};

export default withStyles(styles)(ResizableDrawer);
