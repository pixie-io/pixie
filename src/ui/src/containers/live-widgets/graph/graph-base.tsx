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

import {
  Remove as ZoomOutIcon,
  Add as ZoomInIcon,
} from '@mui/icons-material';
import { IconButton, Tooltip } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Network } from 'vis-network/standalone';

import { buildClass } from 'app/components';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    // Bust out of the extra padding from the <Paper/> container
    width: `calc(100% + ${theme.spacing(1.5)})`,
    margin: theme.spacing(-0.75),
    marginTop: 0,
    borderBottomLeftRadius: 'inherit',
    borderBottomRightRadius: 'inherit',

    // Layout
    flex: 1,
    minHeight: 0,
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'flex-end',
    '&:hover $zoomButton': { opacity: 1 }, // See below for how this works
  },
  clickWrapper: {
    width: '100%',
    height: '100%',
    minHeight: 0,
    border: '1px solid transparent',
    borderBottomLeftRadius: 'inherit',
    borderBottomRightRadius: 'inherit',
    '&$focus': { borderColor: theme.palette.foreground.grey2 },
    '& > .vis-active': { boxShadow: 'none' },
  },
  buttonContainer: {
    marginTop: theme.spacing(1),
    '& .MuiIconButton-root': {
      marginRight: theme.spacing(2),
      padding: theme.spacing(0.375), // 3px
    },
  },
  buttonSeparator: {
    display: 'inline-flex',
    width: theme.spacing(2),
    userSelect: 'none',
  },
  zoomButton: {},
  focus: {/* Blank entry so the rule above has something to reference */},
}), { name: 'Graph' });

// 0 is infinitely zoomed out; 1 is 100% / default zoom level. Min/max based on extreme real-world examples.
const MIN_ZOOM = 0.075;
const MAX_ZOOM = 10;

export interface GraphBaseProps {
  network: Network;
  visRootRef: React.MutableRefObject<HTMLDivElement>;
  showZoomButtons?: boolean;
  setExternalControls?: React.RefCallback<React.ReactNode>;
  additionalButtons?: React.ReactNode;
}

export const GraphBase = React.memo<GraphBaseProps>(({
  network,
  visRootRef,
  showZoomButtons = false,
  setExternalControls,
  additionalButtons,
}) => {
  const [focused, setFocused] = React.useState(false);

  const [zoomLevel, setZoomLevel] = React.useState(1);

  // We perceive the difference between zoom levels relative to each other; multiplicative feels better than additive.
  const changeZoom = React.useCallback((dir: -1 | 1) => {
    // Note: network.getViewPosition and network.getScale give wrong results, so instead track zoomLevel directly.
    let scale = zoomLevel * (1.25 ** dir);
    scale = Math.max(MIN_ZOOM, Math.min(scale, MAX_ZOOM));
    setZoomLevel(scale);
    network.moveTo({ scale });
    setTimeout(() => setFocused(true)); // Wait a cycle so the browser finishes focusing the button first.
  }, [network, zoomLevel]);

  const zoomOut = React.useCallback(() => changeZoom(-1), [changeZoom]);
  const zoomIn = React.useCallback(() => changeZoom(1), [changeZoom]);

  React.useEffect(() => {
    if (!network) return;

    const onStable = () => {
      setZoomLevel(network.getScale());
    };

    // Limit the zoom level in both directions, and remember it so the buttons below can update it properly.
    let lastScalePos = network.getViewPosition(); // One scale event behind, so it can restore smoothly when clamping
    const onZoom = ({ scale }) => {
      const clamped = Math.max(MIN_ZOOM, Math.min(scale, MAX_ZOOM));

      if (clamped !== scale) network.moveTo({ position: lastScalePos, scale: clamped });
      else lastScalePos = network.getViewPosition();

      setZoomLevel(clamped);
    };

    network.on('stabilizationIterationsDone', onStable);
    network.on('zoom', onZoom);

    return () => {
      network.off('stabilizationIterationsDone', onStable);
      network.off('zoom', onZoom);
    };
  }, [network]);

  const [clickWrapper, setClickWrapper] = React.useState<HTMLDivElement>(null);
  const clickWrapperRef = React.useCallback((el: HTMLDivElement) => {
    setClickWrapper(el);
    visRootRef.current = el;
  }, [visRootRef]);

  /*
   * vis-network has a clickToUse option, but it doesn't expose events for focus/blur or a programmatic trigger.
   * So, we reinvent that particular wheel here so we can dynamically control it.
   *
   * The graph is not interactive until it's clicked or touched, and it stops being interactive if Esc is pressed or
   * if the user clicks/taps somewhere else.
   */
  React.useEffect(() => {
    if (!clickWrapper) return;
    const listener = (event: KeyboardEvent | MouseEvent | WheelEvent) => {
      const wasEscPress = event.type === 'keydown' && (event as KeyboardEvent).key === 'Escape';
      const wasClickOrTap = event.type === 'click' || event.type === 'tap';

      if (!focused || wasEscPress) {
        event.stopImmediatePropagation();
      }

      if (focused && wasEscPress) {
        setFocused(false);
      } else if (!focused && wasClickOrTap) {
        setFocused(true);
      }
    };

    // Using the capture phase ensures that this happens before vis-network handles the events.
    const types = ['keydown', 'keyup', 'click', 'wheel', 'tap'];
    for (const t of types) clickWrapper.addEventListener(t, listener, true);

    // Also watch for clicks and the Escape key anywhere on the page that ISN'T targeting a child of clickWrapper
    const clickAwayListener = (event: MouseEvent) => {
      const canvasEl = clickWrapper.querySelector('.vis-network canvas');
      setFocused(canvasEl != null && event.target === canvasEl);
    };
    const escapeListener = (event: KeyboardEvent) => {
      setFocused(event.key === 'Escape' ? false : focused);
    };

    window?.addEventListener('click', clickAwayListener);
    window?.addEventListener('keydown', escapeListener);

    // And make sure to clean up
    return () => {
      for (const t of types) clickWrapper.removeEventListener(t, listener, true);
      window?.removeEventListener('click', clickAwayListener);
      window?.removeEventListener('keydown', escapeListener);
    };
  }, [focused, clickWrapper]);

  const classes = useStyles();

  const controls = React.useMemo(() => (
    <>
      {showZoomButtons && (
        <>
          <Tooltip title='Zoom Out'>
            <IconButton
              className={buildClass(classes.zoomButton, focused && classes.focus)}
              size='small'
              onClick={zoomOut}
              disabled={zoomLevel <= MIN_ZOOM}
            >
              <ZoomOutIcon />
            </IconButton>
          </Tooltip>
          <Tooltip title='Zoom In'>
            <IconButton
              className={buildClass(classes.zoomButton, focused && classes.focus)}
              size='small'
              onClick={zoomIn}
              disabled={zoomLevel >= MAX_ZOOM}
            >
              <ZoomInIcon />
            </IconButton>
          </Tooltip>
          {additionalButtons && <div className={classes.buttonSeparator} />}
        </>
      )}
      {additionalButtons}
    </>
  ), [
    additionalButtons, focused, showZoomButtons, zoomIn, zoomLevel, zoomOut,
    classes.focus, classes.zoomButton, classes.buttonSeparator,
  ]);

  React.useEffect(() => {
    if (setExternalControls) {
      setExternalControls(controls);
    }
  }, [controls, setExternalControls]);

  return (
    <div className={classes.root}>
      <div className={buildClass(classes.clickWrapper, focused && classes.focus)} ref={clickWrapperRef} />
      {!setExternalControls && (
        <div className={classes.buttonContainer}>
          {controls}
        </div>
      )}
    </div>
  );
});
GraphBase.displayName = 'GraphBase';
