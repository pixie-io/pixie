/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { DraggableCore } from 'react-draggable';

import { WithChildren } from 'app/utils/react-boilerplate';

import { FixedSizeDrawer, DrawerDirection } from './drawer';

// The amount of time elasped since a user has last resized the drawer.
const RESIZE_WAIT_INTERVAL_MS = 100;

const useStyles = makeStyles(({ spacing, zIndex }: Theme) => createStyles({
  draggableContent: {
    flex: 1,
    minHeight: 0,
    minWidth: 0,
  },
  verticalDragHandle: {
    cursor: 'row-resize',
    width: '100%',
    height: spacing(1),
    zIndex: zIndex.drawer + 1,
    position: 'absolute',
    pointerEvents: 'auto',
  },
  horizontalDragHandle: {
    cursor: 'col-resize',
    height: '100%',
    width: spacing(1),
    zIndex: zIndex.drawer + 1,
    position: 'absolute',
    pointerEvents: 'auto',
  },
}), { name: 'ResizableDrawer' });

interface ResizableDrawerProps {
  otherContent?: React.ReactNode; // The content that is not in the drawer.
  drawerDirection: DrawerDirection;
  open: boolean;
  initialSize: number;
  overlay: boolean;
  minSize: number;
}

export const ResizableDrawer: React.FC<WithChildren<ResizableDrawerProps>> = React.memo(({
  children,
  otherContent,
  drawerDirection,
  open,
  overlay,
  initialSize,
  minSize,
}) => {
  const classes = useStyles();
  const [drawerSize, setDrawerSize] = React.useState(initialSize);
  // State responsible for tracking whether the user is actively resizing. This is used for debouncing.
  const [timer, setTimer] = React.useState(null);

  const resize = React.useCallback(
    ({ deltaY, deltaX }) => {
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
          setDrawerSize(Math.max(drawerSize - deltaY, minSize));
          break;
        case 'left':
          setDrawerSize(drawerSize + deltaX);
          break;
        case 'right':
          setDrawerSize(Math.max(drawerSize - deltaX, minSize));
          break;
        default:
          break;
      }
    },
    [drawerDirection, drawerSize, timer, minSize],
  );

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
      // eslint-disable-next-line react-memo/require-usememo
      onDrag={(_, { deltaX, deltaY }) => {
        resize({
          deltaY,
          deltaX,
        });
      }}
    >
      <div
        style={dragHandleStyle}
        className={
          drawerDirection === 'top' || drawerDirection === 'bottom'
            ? classes.verticalDragHandle
            : classes.horizontalDragHandle
        }
      />
    </DraggableCore>
  );

  let dragFlex = {};
  if (drawerDirection === 'left' || drawerDirection === 'right') {
    dragFlex = { display: 'flex', flexDirection: 'column' };
  }

  const draggableContent = (
    <div className={classes.draggableContent} style={dragFlex}>
      {(drawerDirection === 'top' || drawerDirection === 'left')
        && open
        && dragHandle}
      {otherContent}
      {(drawerDirection === 'bottom' || drawerDirection === 'right')
        && open
        && dragHandle}
    </div>
  );

  return (
    <>
      <FixedSizeDrawer
        // eslint-disable-next-line react-memo/require-usememo
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
});
ResizableDrawer.displayName = 'ResizableDrawer';
