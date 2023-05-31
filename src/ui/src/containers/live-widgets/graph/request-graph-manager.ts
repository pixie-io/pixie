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

import { data as visData } from 'vis-network/standalone';

import { dataWithUnitsToString, formatBySemType } from 'app/containers/format-data/format-data';
import { WidgetDisplay } from 'app/containers/live/vis';
import { deepLinkURLFromSemanticType, EmbedState } from 'app/containers/live-widgets/utils/live-view-params';
import { Relation, SemanticType } from 'app/types/generated/vizierapi_pb';
import { Arguments } from 'app/utils/args-utils';
import { formatFloat64Data } from 'app/utils/format-data';

import { semTypeToShapeConfig } from './graph-utils';

// An entity in the service graph. Could be a service, pod, or IP address.
export interface Entity {
  id: string;
  label: string;
  // Display configuration properties.
  // `value` determines how to size this node.
  value?: number;
  // `url` contains the entity URL that this node should link to (if any).
  url?: string;
  // `shape` contains shape configuration properties for the node.
  shape?: string;
  // `image` contains the image to use when the node is selected/unselected.
  image?: { selected?: string; unselected?: string };
}

// Statistics about an edge.
interface EdgeStats {
  p50: number;
  p90: number;
  p99: number;
  errorRate: number;
  rps: number;
  inboundBPS: number;
  outboundBPS: number;
  totalRequests: number;
}

// An edge in the service graph. If X is the client in requests to Y,
// and Y is the client in requests to X, then X->Y and Y->X will both be
// present in the edge list.
export interface Edge extends EdgeStats {
  id?: string;
  // These are inherent properies of the edge that we capture from
  // the underlying data.
  from: string;
  to: string;
  // Display configuration parameters.
  // These are properties of the edge that we determine based on visualization
  // parameters.
  value?: number;
  title?: string;
  color?: string;
}

// Defines the input interface for the data we get for a single entry (edge) in the table.
interface InputEdgeInfo extends EdgeStats {
  responderPod: string;
  responderSvc: string;
  responderIP: string;
  requestorPod: string;
  requestorSvc: string;
  requestorIP: string;
}

export interface RequestGraphDisplay extends WidgetDisplay {
  readonly requestorPodColumn: string;
  readonly responderPodColumn: string;
  readonly requestorServiceColumn: string;
  readonly responderServiceColumn: string;
  readonly requestorIPColumn: string;
  readonly responderIPColumn: string;
  readonly p50Column: string;
  readonly p90Column: string;
  readonly p99Column: string;
  readonly errorRateColumn: string;
  readonly requestsPerSecondColumn: string;
  readonly inboundBytesPerSecondColumn: string;
  readonly outboundBytesPerSecondColumn: string;
  readonly totalRequestCountColumn: string;
}

/**
 * Interface for a request graph.
 */
export interface RequestGraph {
  nodes: visData.DataSet<any>;
  edges: visData.DataSet<any>;
}

// Turns a record into an InputEdgeInfo based on the configured columns.
const getEdgeInfo = (value: any, display: RequestGraphDisplay): InputEdgeInfo => ({
  responderPod: value[display.responderPodColumn],
  responderSvc: value[display.responderServiceColumn],
  responderIP: value[display.responderIPColumn],
  requestorPod: value[display.requestorPodColumn],
  requestorSvc: value[display.requestorServiceColumn],
  requestorIP: value[display.requestorIPColumn],
  p50: value[display.p50Column],
  p90: value[display.p90Column],
  p99: value[display.p99Column],
  errorRate: value[display.errorRateColumn],
  rps: value[display.requestsPerSecondColumn],
  inboundBPS: value[display.inboundBytesPerSecondColumn],
  outboundBPS: value[display.outboundBytesPerSecondColumn],
  totalRequests: value[display.totalRequestCountColumn],
});

const humanReadableMetric = (value: any, semType: SemanticType, defaultUnits: string): string => {
  if (semType === SemanticType.ST_NONE || semType === SemanticType.ST_UNSPECIFIED) {
    return `${formatFloat64Data(value)}${defaultUnits}`;
  }
  const valWithUnits = formatBySemType(semType, value);
  return dataWithUnitsToString(valWithUnits);
};

const getEdgeText = (edge: EdgeStats, display: RequestGraphDisplay,
  semTypes: { [key: string]: SemanticType }): string => {
  const bps = edge.inboundBPS + edge.outboundBPS;

  const bpsSemType = semTypes[display.inboundBytesPerSecondColumn];
  const rpsSemType = semTypes[display.requestsPerSecondColumn];
  const errorSemType = semTypes[display.errorRateColumn];
  const p50SemType = semTypes[display.p50Column];
  const p90SemType = semTypes[display.p90Column];
  const p99SemType = semTypes[display.p99Column];

  return [
    humanReadableMetric(bps, bpsSemType, ' B/s'),
    humanReadableMetric(edge.rps, rpsSemType, ' req/s'),
    `Error: ${humanReadableMetric(edge.errorRate, errorSemType, '%')}`,
    `p50: ${humanReadableMetric(edge.p50, p50SemType, 'ms')}`,
    `p90: ${humanReadableMetric(edge.p90, p90SemType, 'ms')}`,
    `p99: ${humanReadableMetric(edge.p99, p99SemType, 'ms')}`,
  ].join('<br>\n');
};

const edgeFromStats = (edge: EdgeStats, from: string, to: string,
  display: RequestGraphDisplay, semTypes: { [key: string]: SemanticType }) => ({
  ...edge,
  from,
  to,
  value: edge.inboundBPS + edge.outboundBPS,
  title: getEdgeText(edge, display, semTypes),
});

// For each edge in the dataset, get the unclustered entities we want to use as the nodes.
// Order of preference for unclustered: pod, service, IP, <unknown>.
const getPod = (pod, svc, ip: string): [string, SemanticType] => {
  if (pod) {
    return [pod, SemanticType.ST_POD_NAME];
  }
  if (svc) {
    return [svc, SemanticType.ST_SERVICE_NAME];
  }
  if (ip) {
    return [ip, SemanticType.ST_IP_ADDRESS];
  }
  return ['<unknown>', SemanticType.ST_NONE];
};

// For each edge in the dataset, get the clustered entities we want to use as the nodes.
// Order of preference for clustered: service, pod, IP, <unknown>.
const getService = (svc, pod, ip: string): [string, SemanticType] => {
  if (svc) {
    return [svc, SemanticType.ST_SERVICE_NAME];
  }
  if (pod) {
    return [pod, SemanticType.ST_POD_NAME];
  }
  if (ip) {
    return [ip, SemanticType.ST_IP_ADDRESS];
  }
  return ['<unknown>', SemanticType.ST_NONE];
};

const upsertNode = (nodes: Map<string, Entity>, name: string, semType: SemanticType,
  bytes: number, selectedClusterName: string, embedState: EmbedState, propagatedArgs?: Arguments) => {
  // Accumulate the total bytes received by this node.
  if (nodes.has(name)) {
    const value = nodes.get(name).value + bytes;
    nodes.set(name, {
      ...nodes.get(name),
      value,
    });
    return;
  }

  const url = deepLinkURLFromSemanticType(semType, name, selectedClusterName, embedState,
    propagatedArgs);

  nodes.set(name, {
    ...semTypeToShapeConfig(semType),
    id: name,
    label: name,
    value: bytes,
    url,
  });
};

// From a collection of edges, estimate the summed edge statistics.
// This is an estimation, not an exact calculation.
// See the following discussion for technique and more information:
// https://stats.stackexchange.com/questions/171784/estimation-of-quantile-given-quantiles-of-subset
const estimateClusteredEdge = (edgeStatsArray: EdgeStats[]): EdgeStats => {
  const clusteredEdgeStats: EdgeStats = {
    p50: 0,
    p90: 0,
    p99: 0,
    errorRate: 0,
    rps: 0,
    inboundBPS: 0,
    outboundBPS: 0,
    totalRequests: 0,
  };
  // Compute the additive statistics.
  edgeStatsArray.forEach((edgeStat: EdgeStats) => {
    clusteredEdgeStats.rps += edgeStat.rps;
    clusteredEdgeStats.inboundBPS += edgeStat.inboundBPS;
    clusteredEdgeStats.outboundBPS += edgeStat.outboundBPS;
    clusteredEdgeStats.totalRequests += edgeStat.totalRequests;
  });
  // Estimate the non-additive statistics.
  edgeStatsArray.forEach((edgeStat: EdgeStats) => {
    // This shouldn't happen, but if the data format we receive back ever changes, we should
    // avoid accidentally dividing by 0.
    if (!clusteredEdgeStats.totalRequests) {
      return;
    }
    const requestRatio = edgeStat.totalRequests / clusteredEdgeStats.totalRequests;
    clusteredEdgeStats.p50 += edgeStat.p50 * requestRatio;
    clusteredEdgeStats.p90 += edgeStat.p90 * requestRatio;
    clusteredEdgeStats.p99 += edgeStat.p99 * requestRatio;
    clusteredEdgeStats.errorRate += edgeStat.errorRate * requestRatio;
  });

  return clusteredEdgeStats;
};

/**
 * Parses the data passed in on the request graph and manages the graph data structure.
 */
export class RequestGraphManager {
  private readonly nodes: visData.DataSet<any>;

  private readonly edges: visData.DataSet<any>;

  private readonly clusteredNodes: visData.DataSet<any>;

  private readonly clusteredEdges: visData.DataSet<any>;

  constructor() {
    this.nodes = new visData.DataSet();
    this.edges = new visData.DataSet();
    this.clusteredNodes = new visData.DataSet();
    this.clusteredEdges = new visData.DataSet();
  }

  // Returns the nodes, clustered by pod.
  // Falls back to service or IP address if pod is not resolved.
  public getRequestGraph(clusteredMode: boolean): RequestGraph {
    if (clusteredMode) {
      return {
        nodes: this.clusteredNodes,
        edges: this.clusteredEdges,
      };
    }
    return {
      nodes: this.nodes,
      edges: this.edges,
    };
  }

  // Sets the edge color of the graph based on an input function.
  public setEdgeColor(colorFn: (edge: Edge) => string): void {
    // Set the edge color for both the clustered and unclustered versions of the graph.
    this.edges.getDataSet().forEach((edge) => {
      this.edges.update({
        ...edge,
        color: colorFn(edge),
      });
    });
    this.clusteredEdges.getDataSet().forEach((edge) => {
      this.clusteredEdges.update({
        ...edge,
        color: colorFn(edge),
      });
    });
  }

  public parseInputData(data: any[], relation: Relation, display: RequestGraphDisplay,
    selectedClusterName: string, embedState?: EmbedState, propagatedArgs?: Arguments): void {
    // Keeps a unique map of the clustered nodes.
    const clusteredNodeMap = new Map<string, Entity>();
    // Keeps a unique map of the clustered edges (since svc1->svc2 may be represented by multiple
    // records).
    const clusteredEdgesMap = new Map<string, Map<string, EdgeStats[]>>();
    // Keeps a unique map of the unclustered nodes.
    const nodeMap = new Map<string, Entity>();
    // Edges grouped by pod/IP are automatically unique,
    // so we don't need an equivalent to `clusteredEdgesMap` here.

    // Capture the semantic types for the columns.
    const semTypes: { [key: string]: SemanticType } = {};
    relation.getColumnsList().forEach((col) => {
      semTypes[col.getColumnName()] = col.getColumnSemanticType();
    });

    // Loop through all the data and create/update pods and edges.
    for (const value of data) {
      const edge: InputEdgeInfo = getEdgeInfo(value, display);

      const [from, fromSemType] = getPod(edge.requestorPod, edge.requestorSvc, edge.requestorIP);
      const [to, toSemType] = getPod(edge.responderPod, edge.responderSvc, edge.responderIP);
      const [fromClustered, fromClusteredSemType] = getService(edge.requestorSvc, edge.requestorPod, edge.requestorIP);
      const [toClustered, toClusteredSemType] = getService(edge.responderSvc, edge.responderPod, edge.responderIP);

      if (!from || !to || !fromClustered || !toClustered) {
        continue;
      }

      // Add this (non-clustered) edge, no more processing needed for this structure.
      this.edges.add(edgeFromStats(edge, from, to, display, semTypes));

      // Initialize the clustered edge maps and add this edge to the map.
      if (!clusteredEdgesMap.has(fromClustered)) {
        clusteredEdgesMap.set(fromClustered, new Map());
      }
      if (!clusteredEdgesMap.get(fromClustered).has(toClustered)) {
        clusteredEdgesMap.get(fromClustered).set(toClustered, []);
      }
      clusteredEdgesMap.get(fromClustered).get(toClustered).push(edge);

      // Ensure the nodes (both clustered and non-clustered) are unique.
      upsertNode(nodeMap, from, fromSemType, edge.outboundBPS, selectedClusterName,
        embedState, propagatedArgs);
      upsertNode(nodeMap, to, toSemType, edge.inboundBPS, selectedClusterName,
        embedState, propagatedArgs);
      upsertNode(clusteredNodeMap, fromClustered, fromClusteredSemType, edge.outboundBPS,
        selectedClusterName, embedState, propagatedArgs);
      upsertNode(clusteredNodeMap, toClustered, toClusteredSemType, edge.inboundBPS,
        selectedClusterName, embedState, propagatedArgs);
    }

    this.finalizeGraph(nodeMap, clusteredNodeMap, clusteredEdgesMap, display, semTypes);
  }

  private finalizeGraph(nodeMap: Map<string, Entity>, clusteredNodeMap: Map<string, Entity>,
    clusteredEdgesMap: Map<string, Map<string, EdgeStats[]>>,
    display: RequestGraphDisplay, semTypes: { [key: string]: SemanticType }): void {
    nodeMap.forEach((value) => {
      this.nodes.add(value);
    });
    clusteredNodeMap.forEach((value) => {
      this.clusteredNodes.add(value);
    });
    clusteredEdgesMap.forEach((subMap, fromClustered) => {
      subMap.forEach((edgeStatsArray, toClustered) => {
        const clusteredEdgeStats = estimateClusteredEdge(edgeStatsArray);
        this.clusteredEdges.add(
          edgeFromStats(clusteredEdgeStats, fromClustered, toClustered,
            display, semTypes));
      });
    });
  }
}
