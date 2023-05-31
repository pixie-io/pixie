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

import { Button } from '@mui/material';
import { useTheme } from '@mui/material/styles';
import { useHistory } from 'react-router-dom';
import {
  data as visData,
  Edge,
  Network,
  Node,
  parseDOTNetwork,
} from 'vis-network/standalone';

import { ClusterContext } from 'app/common/cluster-context';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { WidgetDisplay } from 'app/containers/live/vis';
import { Relation, SemanticType } from 'app/types/generated/vizierapi_pb';
import { Arguments } from 'app/utils/args-utils';
import { GaugeLevel, getColor, getLatencyNSLevel } from 'app/utils/metric-thresholds';

import { GraphBase } from './graph-base';
import {
  ColInfo,
  colInfoFromName,
  getGraphOptions,
  semTypeToShapeConfig,
} from './graph-utils';
import { formatByDataType, formatBySemType } from '../../format-data/format-data';
import { deepLinkURLFromSemanticType } from '../utils/live-view-params';

interface AdjacencyList {
  toColumn: string;
  fromColumn: string;
}

interface EdgeThresholds {
  mediumThreshold: number;
  highThreshold: number;
}

export interface GraphDisplay extends WidgetDisplay {
  readonly dotColumn?: string;
  readonly adjacencyList?: AdjacencyList;
  readonly data?: any[];
  readonly edgeWeightColumn?: string;
  readonly nodeWeightColumn?: string;
  readonly edgeColorColumn?: string;
  readonly edgeThresholds?: EdgeThresholds;
  readonly edgeHoverInfo?: string[];
  readonly edgeLength?: number;
  readonly enableDefaultHierarchy?: boolean;
}

interface GraphProps {
  dot?: any;
  data?: any[];
  toCol?: ColInfo;
  fromCol?: ColInfo;
  propagatedArgs?: Arguments;
  edgeWeightColumn?: string;
  nodeWeightColumn?: string;
  edgeColorColumn?: ColInfo;
  edgeThresholds?: EdgeThresholds;
  edgeHoverInfo?: ColInfo[];
  edgeLength?: number;
  enableDefaultHierarchy?: boolean;
  setExternalControls?: React.RefCallback<React.ReactNode>;
}

interface GraphData {
  nodes: visData.DataSet<Node>;
  edges: visData.DataSet<Edge>;
  idToSemType: { [ key: string ]: SemanticType };
  propagatedArgs?: Arguments;
}

interface GraphWidgetProps {
  display: GraphDisplay;
  data: any[];
  relation: Relation;
  propagatedArgs?: Arguments;
  setExternalControls?: React.RefCallback<React.ReactNode>;
}

const INVALID_NODE_TYPES = [
  SemanticType.ST_SCRIPT_REFERENCE,
  SemanticType.ST_HTTP_RESP_MESSAGE,
];

const LATENCY_TYPES = [
  SemanticType.ST_DURATION_NS,
  SemanticType.ST_THROUGHPUT_PER_NS,
  SemanticType.ST_THROUGHPUT_BYTES_PER_NS,
];

function getColorForEdge(col: ColInfo, val: number, thresholds: EdgeThresholds): GaugeLevel {
  if (!thresholds && LATENCY_TYPES.includes(col.semType)) {
    return getLatencyNSLevel(val);
  }

  const medThreshold = thresholds ? thresholds.mediumThreshold : 100;
  const highThreshold = thresholds ? thresholds.highThreshold : 200;

  if (val < medThreshold) {
    return 'low';
  }
  return val > highThreshold ? 'high' : 'med';
}

export const Graph = React.memo<GraphProps>(({
  dot, toCol, fromCol, data, propagatedArgs, edgeWeightColumn,
  nodeWeightColumn, edgeColorColumn, edgeThresholds, edgeHoverInfo, edgeLength, enableDefaultHierarchy,
  setExternalControls,
}) => {
  const theme = useTheme();

  const { selectedClusterName } = React.useContext(ClusterContext);
  const history = useHistory();

  const [hierarchyEnabled, setHierarchyEnabled] = React.useState<boolean>(enableDefaultHierarchy);
  const [network, setNetwork] = React.useState<Network>(null);
  const [graph, setGraph] = React.useState<GraphData>(null);

  const { embedState } = React.useContext(LiveRouteContext);

  const doubleClickCallback = React.useCallback((params?: any) => {
    if (params.nodes.length > 0 && !embedState.widget) {
      const nodeID = params.nodes[0];
      const semType = graph.idToSemType[nodeID];
      const url = deepLinkURLFromSemanticType(semType, nodeID, selectedClusterName, embedState,
        propagatedArgs);
      if (url) {
        history.push(url);
      }
    }
  }, [history, selectedClusterName, graph, propagatedArgs, embedState]);

  const ref = React.useRef<HTMLDivElement>();

  const toggleHierarchy = React.useCallback(() => {
    setHierarchyEnabled(!hierarchyEnabled);
  }, [hierarchyEnabled]);

  // Load the graph.
  React.useEffect(() => {
    if (dot) {
      const dotData = parseDOTNetwork(dot);
      setGraph(dotData);
      return;
    }

    const edges = new visData.DataSet<Edge>();
    const nodes = new visData.DataSet<Node>();
    const idToSemType = {};

    const upsertNode = (label: string, st: SemanticType, weight: number) => {
      if (!idToSemType[label]) {
        const node = {
          ...semTypeToShapeConfig(st),
          id: label,
          label,
        };

        if (weight !== -1) {
          node.value = weight;
        }

        nodes.add(node);
        idToSemType[label] = st;
      }
    };
    data.forEach((d) => {
      const nt = d[toCol.name];
      const nf = d[fromCol.name];

      let nodeWeight = -1;
      if (nodeWeightColumn && nodeWeightColumn !== '') {
        nodeWeight = d[nodeWeightColumn];
      }

      upsertNode(nt, toCol?.semType, nodeWeight);
      upsertNode(nf, fromCol?.semType, nodeWeight);

      const edge = {
        from: nf,
        to: nt,
      } as Edge;

      if (edgeWeightColumn && edgeWeightColumn !== '') {
        edge.value = d[edgeWeightColumn];
      }

      if (edgeColorColumn) {
        const level = getColorForEdge(edgeColorColumn, d[edgeColorColumn.name], edgeThresholds);
        edge.color = getColor(level, theme);
      }

      if (edgeHoverInfo && edgeHoverInfo.length > 0) {
        let edgeInfo = '';
        edgeHoverInfo.forEach((info, i) => {
          if (info != null) {
            let val: string;
            if (info.semType === SemanticType.ST_NONE || info.semType === SemanticType.ST_UNSPECIFIED) {
              val = formatByDataType(info.type, d[info.name]);
            } else {
              const valWithUnits = formatBySemType(info.semType, d[info.name]);
              val = `${valWithUnits.val} ${valWithUnits.units}`;
            }
            edgeInfo = `${edgeInfo}${i === 0 ? '' : '<br>'} ${info.name}: ${val}`;
          }
        });
        edge.title = edgeInfo;
      }

      edges.add(edge);
    });

    setGraph({
      nodes, edges, idToSemType,
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [dot, data, toCol, fromCol]);

  // Load the data.
  React.useEffect(() => {
    if (!graph) {
      return;
    }
    const opts = getGraphOptions(theme, edgeLength);

    if (hierarchyEnabled) {
      opts.layout.hierarchical = {
        enabled: true,
        levelSeparation: 400,
        nodeSpacing: 10,
        treeSpacing: 50,
        direction: 'LR',
        sortMethod: 'directed',
      };
    }

    const n = new Network(ref.current, graph, opts);
    n.on('doubleClick', doubleClickCallback);

    n.on('stabilizationIterationsDone', () => {
      n.setOptions({ physics: false });
    });
    setNetwork(n);

  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graph, doubleClickCallback, hierarchyEnabled]);

  const controls = React.useMemo(() => (
    <Button
      size='small'
      onClick={toggleHierarchy}
    >
      {hierarchyEnabled ? 'Disable hierarchy' : 'Enable hierarchy'}
    </Button>
  ), [hierarchyEnabled, toggleHierarchy]);

  return (
    <GraphBase
      network={network}
      visRootRef={ref}
      showZoomButtons={true}
      setExternalControls={setExternalControls}
      additionalButtons={controls}
    />
  );
});
Graph.displayName = 'Graph';

export const GraphWidget = React.memo<GraphWidgetProps>(({
  display, data, relation, propagatedArgs, setExternalControls,
}) => {
  if (display.dotColumn && data.length > 0) {
    return (
      <Graph dot={data[0][display.dotColumn]} />
    );
  } if (display.adjacencyList && display.adjacencyList.fromColumn && display.adjacencyList.toColumn) {
    let errorMsg = '';

    const toColInfo = colInfoFromName(relation, display.adjacencyList.toColumn);
    if (toColInfo && INVALID_NODE_TYPES.includes(toColInfo.semType)) {
      errorMsg = `${display.adjacencyList.toColumn} cannot be used as the source column`;
    }
    const fromColInfo = colInfoFromName(relation, display.adjacencyList.fromColumn);
    if (fromColInfo && INVALID_NODE_TYPES.includes(fromColInfo.semType)) {
      errorMsg = `${display.adjacencyList.fromColumn} cannot be used as the destination column`;
    }
    const colorColInfo = colInfoFromName(relation, display.edgeColorColumn);
    const edgeHoverInfo = [];
    if (display.edgeHoverInfo && display.edgeHoverInfo.length > 0) {
      for (const e of display.edgeHoverInfo) {
        const info = colInfoFromName(relation, e);
        if (info) { // Only push valid column infos. The user may pass in an invalid column name in the vis spec.
          edgeHoverInfo.push(info);
        }
      }
    }
    if (toColInfo && fromColInfo && !errorMsg) {
      return (
        <Graph
          {...display}
          data={data}
          toCol={toColInfo}
          fromCol={fromColInfo}
          edgeColorColumn={colorColInfo}
          propagatedArgs={propagatedArgs}
          edgeHoverInfo={edgeHoverInfo}
          setExternalControls={setExternalControls}
        />
      );
    }

    if (!toColInfo) {
      errorMsg = `${display.adjacencyList.toColumn} column does not exist`;
    } else if (!fromColInfo) {
      errorMsg = `${display.adjacencyList.fromColumn} column does not exist`;
    }

    return <div>{errorMsg}</div>;
  }
  return <div key={display.dotColumn}>Invalid spec for graph</div>;
});
GraphWidget.displayName = 'GraphWidget';
