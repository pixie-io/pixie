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
import { WidgetDisplay } from 'app/containers/live/vis';

import { data as visData, Network, Options } from 'vis-network/standalone';
import { makeStyles, useTheme, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import AccountTreeIcon from '@material-ui/icons/AccountTree';
import WorkspacesIcon from '@material-ui/icons/Workspaces';
import SpeedIcon from '@material-ui/icons/Speed';
import ErrorOutlineIcon from '@material-ui/icons/ErrorOutline';
import IconButton from '@material-ui/core/IconButton';
import Tooltip from '@material-ui/core/Tooltip';
import { toEntityURL, toSingleEntityPage } from 'app/containers/live-widgets/utils/live-view-params';
import { ClusterContext } from 'app/common/cluster-context';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { SemanticType, Relation } from 'app/types/generated/vizierapi_pb';
import { useHistory } from 'react-router-dom';
import { Arguments } from 'app/utils/args-utils';
import { formatFloat64Data } from 'app/utils/format-data';
import {
  getColorForErrorRate,
  getColorForLatency,
  getGraphOptions,
  LABEL_OPTIONS as labelOpts,
  semTypeToShapeConfig,
  colInfoFromName,
  ColInfo,
} from './graph-utils';
import { Edge, RequestGraph, RequestGraphParser } from './request-graph-parser';
import { formatBySemType } from '../../format-data/format-data';

export interface RequestGraphDisplay extends WidgetDisplay {
  readonly requestorPodColumn: string;
  readonly responderPodColumn: string;
  readonly requestorServiceColumn: string;
  readonly responderServiceColumn: string;
  readonly p50Column: string;
  readonly p90Column: string;
  readonly p99Column: string;
  readonly errorRateColumn: string;
  readonly requestsPerSecondColumn: string;
  readonly inboundBytesPerSecondColumn: string;
  readonly outboundBytesPerSecondColumn: string;
  readonly totalRequestCountColumn: string;
}

interface RequestGraphProps {
  display: RequestGraphDisplay;
  data: any[];
  relation: Relation;
  propagatedArgs?: Arguments;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    width: '100%',
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'flex-end',
    '&.focus': {
      border: '1px solid #353738',
    },
  },
  container: {
    width: '100%',
    height: '90%',
    minHeight: 0,
    '& > .vis-active': {
      boxShadow: 'none',
    },
  },
  enabled: {
    color: theme.palette.text.secondary,
  },
  buttonContainer: {
    '& > .MuiIconButton-root': {
      marginRight: theme.spacing(2),
    },
  },
}));

export const RequestGraphWidget: React.FC<RequestGraphProps> = ({
  data, relation, display, propagatedArgs,
}) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const history = useHistory();

  const ref = React.useRef<HTMLDivElement>();

  const [network, setNetwork] = React.useState<Network>(null);
  const [graph, setGraph] = React.useState<RequestGraph>(null);

  const [colInfos, setColInfos] = React.useState<{ [key: string]: ColInfo }>({});

  const [clusteredMode, setClusteredMode] = React.useState<boolean>(true);
  const [hierarchyEnabled, setHierarchyEnabled] = React.useState<boolean>(false);
  const [colorByLatency, setColorByLatency] = React.useState<boolean>(false);
  const [focused, setFocused] = React.useState<boolean>(false);

  const theme = useTheme();
  const defaultGraphOpts = getGraphOptions(theme, -1);
  const [graphOpts, setGraphOpts] = React.useState<Options>(defaultGraphOpts);

  /**
   * Toggle the hier/non-hier clustering mode.
   */
  const toggleMode = React.useCallback(() => setClusteredMode(
    (clustered) => !clustered), [setClusteredMode]);

  React.useEffect(() => {
    const infos = {};

    // Get columnInfos for all relevant columns.
    const cols = [display.p50Column, display.p90Column, display.errorRateColumn, display.requestsPerSecondColumn,
      display.inboundBytesPerSecondColumn, display.outboundBytesPerSecondColumn,
    ];

    cols.forEach((c) => {
      infos[c] = colInfoFromName(relation, c);
    });

    setColInfos(infos);
  }, [relation, display]);

  React.useEffect(() => {
    if (network && graph) {
      network.setData({
        nodes: graph.nodes,
        edges: graph.edges,
      });
      if (clusteredMode) {
        // Clustered.
        graph.services.forEach((svc) => {
          const clusterOptionsByData = {
            joinCondition(childOptions) {
              return childOptions.service === svc;
            },
            clusterNodeProperties: {
              ...semTypeToShapeConfig(SemanticType.ST_SERVICE_NAME),
              allowSingleNodeCluster: true,
              label: svc,
              scaling: labelOpts,
            },
            processProperties(clusterOptions,
              childNodes) {
              const newOptions = clusterOptions;
              let totalValue = 0;
              childNodes.forEach((node) => {
                totalValue += node.value;
              });
              newOptions.value = totalValue;
              if (svc === '') {
                newOptions.hidden = true;
                newOptions.physics = false;
              }
              return newOptions;
            },
          };
          network.cluster(clusterOptionsByData);
        });
      }
    }
  }, [network, graph, clusteredMode]);

  /**
   * This is used to toggle the hierarchical state of graph.
   */
  const toggleHierarchy = React.useCallback(() => setHierarchyEnabled((enabled) => {
    const hierEnabled = !enabled;
    let opts = {};
    if (hierEnabled) {
      opts = {
        ...defaultGraphOpts,
        layout: {
          ...defaultGraphOpts.layout,
          hierarchical: {
            enabled: true,
            levelSeparation: 50,
            direction: 'LR',
            sortMethod: 'directed',
            nodeSpacing: 50,
            edgeMinimization: true,
            blockShifting: true,
          },
        },
      };
    } else {
      opts = {
        ...defaultGraphOpts,
        layout: {
          ...defaultGraphOpts.layout,
          hierarchical: {
            enabled: false,
          },
        },
      };
    }
    setGraphOpts(opts);
    return hierEnabled;
  }), [defaultGraphOpts]);

  const toggleColor = React.useCallback(() => setColorByLatency((enabled) => {
    const latencyColor = !enabled;
    if (graph) {
      graph.edges.forEach((edge: Edge) => {
        graph.edges.update({
          ...edge,
          color: latencyColor ? getColorForLatency(edge.p99, theme) : getColorForErrorRate(edge.errorRate, theme),
        });
      });
    }
    return latencyColor;
  }), [theme, graph]);

  const toggleFocus = React.useCallback(() => setFocused((enabled) => !enabled), []);

  // Load the graph.
  React.useEffect(() => {
    const p = new RequestGraphParser(data, display);
    const nodeDS = new visData.DataSet();
    nodeDS.add(p.getEntities());
    const edgeDS = new visData.DataSet();
    edgeDS.add(p.getEdges());
    setGraph({
      nodes: nodeDS,
      edges: edgeDS,
      services: p.getServiceList(),
    });
  }, [data, display]);

  // Load the data.
  React.useEffect(() => {
    if (ref && graph) {
      // Hydrate the data.

      const getDisplayText = (value: any, colName: string, defaultUnits: string) => {
        const info = colInfos[colName];
        if (info?.semType === SemanticType.ST_NONE || info?.semType === SemanticType.ST_UNSPECIFIED) {
          return `${formatFloat64Data(value)}${defaultUnits}`;
        }
        const valWithUnits = formatBySemType(info?.semType, value);
        return `${valWithUnits.val} ${valWithUnits.units}`;
      };

      graph.edges.forEach((edge: Edge) => {
        const bps = edge.inboundBPS + edge.outputBPS;

        const title = `${getDisplayText(bps, display.inboundBytesPerSecondColumn, ' B/s')} <br>
                       ${getDisplayText(edge.rps, display.requestsPerSecondColumn, ' req/s')} <br>
                       Error: ${getDisplayText(edge.errorRate, display.errorRateColumn, '%')} <br>
                       p50: ${getDisplayText(edge.p50, display.p50Column, 'ms')} <br>
                       p90: ${getDisplayText(edge.p90, display.p50Column, 'ms')}
        `;

        const color = colorByLatency
          ? getColorForLatency(edge.p99, theme) : getColorForErrorRate(edge.errorRate, theme);
        const value = bps;
        graph.edges.update({
          ...edge,
          title,
          color,
          value,
        });
      });

      const d = {
        nodes: graph.nodes,
        edges: graph.edges,
      };
      const n = new Network(ref.current, d, graphOpts);

      n.on('stabilizationIterationsDone', () => {
        n.setOptions({ physics: false });
      });
      setNetwork(n);
    }
    // To list all exhaustive deps, we also have to list theme, which will never change.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graph, display, ref, colorByLatency, colInfos, graphOpts]);

  const { embedState } = React.useContext(LiveRouteContext);

  const doubleClickCallback = React.useCallback((params?: any) => {
    if (params.nodes.length > 0 && !embedState.widget) {
      const nodeName = !clusteredMode ? params.nodes[0]
        : graph.nodes.get(network.getNodesInCluster(params.nodes[0]))[0].service;
      const semType = !clusteredMode ? SemanticType.ST_POD_NAME : SemanticType.ST_SERVICE_NAME;
      const page = toSingleEntityPage(nodeName, semType, selectedClusterName);
      const pathname = toEntityURL(page, embedState, propagatedArgs);
      history.push(pathname);
    }
  }, [history, selectedClusterName, clusteredMode, network, graph, propagatedArgs, embedState]);

  // This function needs to dynamically change on 'network' every time clusteredMode is updated,
  // so we assign it separately from where Network is created.
  React.useEffect(() => {
    if (network) {
      network.off('doubleClick'); // Clear the previous doubleClick listener.
      network.on('doubleClick', doubleClickCallback);
    }
  }, [network, doubleClickCallback]);

  const classes = useStyles();
  return (
    <div className={`${classes.root} ${focused ? 'focus' : ''}`} onFocus={toggleFocus} onBlur={toggleFocus}>
      <div className={classes.container} ref={ref} />
      <div className={classes.buttonContainer}>
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
};
RequestGraphWidget.defaultProps = {
  propagatedArgs: null,
};
