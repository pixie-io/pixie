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
import { SemanticType } from 'app/types/generated/vizierapi_pb';
import { getNamespaceFromEntityName, semTypeToShapeConfig } from './graph-utils';
import { RequestGraphDisplay } from './request-graph';

export interface Entity {
  id: string;
  label: string;
  namespace?: string;
  service?: string;
  pod?: string;

  // Determines how to size this node.
  value?: number;

  // Display configuration.
  shape?: string;
  image?: { selected?: string; unselected?: string };
}

export interface Edge {
  id?: string;
  // These are inherent properies of the edge that we capture from
  // the underlying data.
  from: string;
  to: string;
  p50?: number;
  p90?: number;
  p99?: number;
  errorRate?: number;
  rps?: number;
  inboundBPS?: number;
  outputBPS?: number;

  // These are properties of the edge that we determine based on visualization
  // parameters.
  value?: number;
  title?: string;
  color?: string;
}

export interface RequestGraph {
  nodes: visData.DataSet<any>;
  edges: visData.DataSet<any>;
  services: string[];
}

/**
 * Parses the data passed in on the request graph.
 */
export class RequestGraphParser {
  private readonly entities: Entity[] = [];

  private readonly edges: Edge[] = [];

  // Keeps a mapping from pod to node. We use the pod name as identifier
  // since it's the lowest level and guaranteed to be unique.
  private readonly pods = {};

  // We use this map to store if we already have this service in the
  // entity array;
  private hasSvc = {};

  constructor(data: any[], display: RequestGraphDisplay) {
    this.parseInputData(data, display);
  }

  public getEdges(): Edge[] {
    return this.edges;
  }

  public getEntities(): Entity[] {
    return this.entities;
  }

  public getServiceList(): string[] {
    return Object.keys(this.hasSvc);
  }

  private parseInputData(data: any[], display: RequestGraphDisplay) {
    // Loop through all the data and create/update pods and edges.
    data.forEach((value) => {
      const req = this.upsertPod(
        value[display.requestorServiceColumn],
        value[display.requestorPodColumn] || '<unknown service>',
        value[display.outboundBytesPerSecondColumn],
      );
      const resp = this.upsertPod(
        value[display.responderServiceColumn],
        value[display.responderPodColumn] || '<unknown service>',
        value[display.inboundBytesPerSecondColumn],
      );

      this.edges.push({
        from: req.id,
        to: resp.id,
        p50: value[display.p50Column],
        p90: value[display.p90Column],
        p99: value[display.p99Column],
        errorRate: value[display.errorRateColumn],
        rps: value[display.requestsPerSecondColumn],
        inboundBPS: value[display.inboundBytesPerSecondColumn],
        outputBPS: value[display.outboundBytesPerSecondColumn],
      });
    });
  }

  /**
   * Upsert pod creates or updates the existing pod.
   *
   * The only value we accumulate is the BPS which is used as a proxy to
   * size the pods.
   * @param svc The name of the service.
   * @param pod The name of the Pod.
   * @param bps The bytes/second of traffic to this pod.
   */
  private upsertPod(svc: string, pod: string, bps: number): Entity {
    if (this.pods[pod]) {
      this.pods[pod].value += bps;
      return this.pods[pod];
    }

    const newPod = {
      ...semTypeToShapeConfig(SemanticType.ST_POD_NAME),
      id: pod,
      label: pod,
      namespace: getNamespaceFromEntityName(pod),
      service: svc,
      pod,
      value: bps,
    };

    this.hasSvc[svc] = true;
    this.pods[pod] = newPod;
    this.entities.push(newPod);
    return newPod;
  }
}
