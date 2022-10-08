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
  AccountTree as AccountTreeIcon,
  Workspaces as WorkspacesIcon,
  Speed as SpeedIcon,
  ErrorOutline as ErrorOutlineIcon,
  Remove as ZoomOutIcon,
  Add as ZoomInIcon,
} from '@mui/icons-material';
import { IconButton, Tooltip } from '@mui/material';
import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useHistory } from 'react-router-dom';
import { Network } from 'vis-network/standalone';

import { ClusterContext } from 'app/common/cluster-context';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { Relation } from 'app/types/generated/vizierapi_pb';
import { Arguments } from 'app/utils/args-utils';
import { buildClass } from 'app/utils/build-class';

import {
  getColorForErrorRate,
  getColorForLatency,
  getGraphOptions,
} from './graph-utils';
import {
  Edge, Entity, RequestGraphDisplay, RequestGraphManager,
} from './request-graph-manager';

interface RequestGraphProps {
  display: RequestGraphDisplay;
  data: any[];
  relation: Relation;
  propagatedArgs?: Arguments;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    width: '100%',
    flex: 1,
    minHeight: 0,
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'flex-end',
    '&:hover $zoomButton': { opacity: 1 }, // See below for how this works
  },
  container: {
    width: '100%',
    height: '100%',
    minHeight: 0,
    border: '1px solid transparent',
    // https://cssinjs.org/jss-plugin-nested/#use-rulename-to-reference-a-local-rule-within-the-same-style-sheet
    '&$focus': { borderColor: theme.palette.foreground.grey2 },
    '& > .vis-active': {
      boxShadow: 'none',
    },
  },
  focus: {/* Blank entry so the rule above has something to reference */},
  enabled: {
    color: theme.palette.text.secondary,
  },
  buttonContainer: {
    marginTop: theme.spacing(1),
    '& .MuiIconButton-root': {
      marginRight: theme.spacing(2),
      padding: theme.spacing(0.375), // 3px
    },
  },
  zoomButton: {
    opacity: 0,
    transition: 'opacity 0.25s linear',
    '&$focus': { opacity: 1 },
  },
  zoomButtonLast: { marginRight: theme.spacing(2) }, // An extra half of a button's width
}), { name: 'RequestGraphWidget' });

// 0 is infinitely zoomed out; 1 is 100% / default zoom level.
const MIN_ZOOM = 0.25;
const MAX_ZOOM = 10;

// We perceive the difference between zoom levels relative to each other; multiplicative feels better than additive.
function computeZoom(scale: number, adjust: number): number {
  const newVal = adjust < 0 ? scale / 1.25 : scale * 1.25;
  return Math.max(MIN_ZOOM, Math.min(newVal, MAX_ZOOM));
}

export const RequestGraphWidget = React.memo<RequestGraphProps>(({
  data, relation, display, propagatedArgs,
}) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const { embedState } = React.useContext(LiveRouteContext);
  const history = useHistory();

  const ref = React.useRef<HTMLDivElement>();

  const [network, setNetwork] = React.useState<Network>(null);
  const [graphMgr, setGraphMgr] = React.useState<RequestGraphManager>(null);

  const [clusteredMode, setClusteredMode] = React.useState<boolean>(true);
  const [hierarchyEnabled, setHierarchyEnabled] = React.useState<boolean>(false);
  const [colorByLatency, setColorByLatency] = React.useState<boolean>(false);
  const [focused, setFocused] = React.useState<boolean>(false);
  const [zoomLevel, setZoomLevel] = React.useState(1);

  const theme = useTheme();
  /**
   * Toggle the clustering of the graph (service vs pod).
   */
  const toggleMode = React.useCallback(() => setClusteredMode((clustered) => !clustered), []);

  /**
   * Toggle the hierarchical state of the graph.
   */
  const toggleHierarchy = React.useCallback(() => setHierarchyEnabled((enabled) => !enabled), []);

  const getEdgeColoringFn = React.useCallback((latencyColor: boolean) => ((edge: Edge): string => {
    if (latencyColor) {
      return getColorForLatency(edge.p99, theme);
    }
    return getColorForErrorRate(edge.errorRate, theme);
  }), [theme]);

  const defaultGraphOpts = React.useMemo(() => getGraphOptions(theme, -1), [theme]);

  /**
   * Toggle the color mode of the graph.
   */
  const toggleColor = React.useCallback(() => setColorByLatency((enabled) => {
    const latencyColor = !enabled;
    if (graphMgr) {
      graphMgr.setEdgeColor(getEdgeColoringFn(latencyColor));
    }
    return latencyColor;
  }), [graphMgr, getEdgeColoringFn]);

  /**
   * Toggle whether the graph is in focus.
   */
  const toggleFocus = React.useCallback(() => setFocused((enabled) => !enabled), []);

  /**
   * Load data when the data or display changes.
   */
  React.useEffect(() => {
    const gMgr = new RequestGraphManager();
    gMgr.parseInputData(data, relation, display, selectedClusterName, embedState, propagatedArgs);
    gMgr.setEdgeColor(getEdgeColoringFn(colorByLatency));
    setGraphMgr(gMgr);
  // We don't actually want to redo this whenever colorByLatency changes, that will be
  // handled by the next React.useEffect. This is for initial load only.
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [data, display, relation, selectedClusterName, embedState, propagatedArgs, getEdgeColoringFn]);

  /**
   * Reload the network when the graph substantially changes. This allows the proper
   * new layout to be computed and handle the physics logic in one place.
   */
  React.useEffect(() => {
    if (!graphMgr) {
      return;
    }

    const graphOpts = {
      ...defaultGraphOpts,
      layout: {
        ...defaultGraphOpts.layout,
        hierarchical: hierarchyEnabled ? {
          enabled: true,
          levelSeparation: 50,
          direction: 'UD',
          sortMethod: 'directed',
          nodeSpacing: 50,
          edgeMinimization: true,
          blockShifting: true,
        } : {
          enabled: false,
        },
      },
    };

    const n = new Network(ref.current, graphMgr.getRequestGraph(clusteredMode), graphOpts);
    n.on('stabilizationIterationsDone', () => {
      n.setOptions({ physics: false });
    });

    // Limit the zoom level in both directions, and remember it so the buttons below can update it properly.
    let lastScalePos = n.getViewPosition(); // One scale event behind, so it can restore smoothly when clamping
    n.on('zoom', ({ scale }) => {
      const clamped = Math.min(MAX_ZOOM, Math.max(scale, MIN_ZOOM));

      if (clamped !== scale) n.moveTo({ position: lastScalePos, scale: clamped });
      else lastScalePos = n.getViewPosition();

      setZoomLevel(clamped);
    });
    setNetwork(n);
  }, [graphMgr, clusteredMode, hierarchyEnabled, defaultGraphOpts]);

  const doubleClickCallback = React.useCallback((params?: any) => {
    if (params.nodes.length > 0 && !embedState.widget) {
      // Unfortunately, vis's getRequestGraph has different signatures based on the input.
      // For this input, it will return a single Entity node, but for other inputs it might
      // return an any[], and that is the type that TypeScript is receiving. So, we have to
      // do this ugly conversion here. See `get` in https://visjs.github.io/vis-data/data/dataset.html.
      const node = graphMgr.getRequestGraph(clusteredMode).nodes.get(params.nodes[0]) as unknown as Entity;
      if (node?.url) {
        history.push(node.url);
      }
    }
  }, [graphMgr, clusteredMode, history, embedState]);

  // This function needs to dynamically change on 'network' every time clusteredMode is updated,
  // so we assign it separately from where Network is created.
  React.useEffect(() => {
    if (network) {
      network.off('doubleClick'); // Clear the previous doubleClick listener.
      network.on('doubleClick', doubleClickCallback);
    }
  }, [network, doubleClickCallback]);

  // Note: network.getViewPosition and network.getScale yield wildly varying values between invocations, so instead
  // we track zoomLevel directly from the on('zoom') handler above and let the position stay the same.
  // The amount we change the zoom level by is nonlinear, because 4x zoom doesn't actually feel like it's 4x closer.

  const zoomOut = React.useCallback(() => {
    const scale = computeZoom(zoomLevel, -1);
    setZoomLevel(scale);
    network.moveTo({ scale });
  }, [network, zoomLevel]);

  const zoomIn = React.useCallback(() => {
    const scale = computeZoom(zoomLevel, 1);
    setZoomLevel(scale);
    network.moveTo({ scale });
  }, [network, zoomLevel]);

  const classes = useStyles();
  return (
    <div className={classes.root} onFocus={toggleFocus} onBlur={toggleFocus}>
      <div className={buildClass(classes.container, focused && classes.focus)} ref={ref} />
      <div className={classes.buttonContainer}>
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
        <Tooltip title={colorByLatency ? 'Colored by latency' : 'Colored by Error Rate'}>
          <IconButton
            size='small'
            onClick={toggleColor}
            className={classes.enabled}
          >
            {colorByLatency ? <SpeedIcon /> : <ErrorOutlineIcon />}
          </IconButton>
        </Tooltip>
        <Tooltip title={hierarchyEnabled ? 'Hierarchy enabled' : 'Hierarchy disabled'}>
          <IconButton
            size='small'
            onClick={toggleHierarchy}
            className={hierarchyEnabled ? classes.enabled : ''}
          >
            <AccountTreeIcon />
          </IconButton>
        </Tooltip>
        <Tooltip title={clusteredMode ? 'Clustered by service' : 'Clustering disabled'}>
          <IconButton
            size='small'
            onClick={toggleMode}
            className={clusteredMode ? classes.enabled : ''}
          >
            <WorkspacesIcon />
          </IconButton>
        </Tooltip>
      </div>
    </div>
  );
});
RequestGraphWidget.displayName = 'RequestGraphWidget';
