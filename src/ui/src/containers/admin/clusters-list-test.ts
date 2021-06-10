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

import { GQLClusterInfo, GQLClusterStatus } from 'app/types/schema';
import { formatClusters } from './clusters-list';

describe('formatClusters', () => {
  it('correctly formats cluster info', () => {
    const clusterResults: GQLClusterInfo[] = [
      {
        id: '5b27f024-1234-4d07-b28d-84ab8d88e1a3',
        clusterUID: '5b27f024-1234-4d07-b28d-84ab8d88e1a3',
        clusterName: '456',
        prettyClusterName: 'pretty-456',
        status: GQLClusterStatus.CS_DISCONNECTED,
        clusterVersion: 'versionABC',
        vizierVersion: '0.2.4-pre-master.64',
        vizierConfig: {
          passthroughEnabled: false,
        },
        lastHeartbeatMs: 23424332349024.02,
        numNodes: 0,
        numInstrumentedNodes: 4,
        controlPlanePodStatuses: [],
      },
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        clusterUID: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        clusterName: '123',
        prettyClusterName: 'pretty-123',
        status: GQLClusterStatus.CS_HEALTHY,
        clusterVersion: 'versionABC',
        vizierVersion: '0.2.4-pre-master.64',
        vizierConfig: {
          passthroughEnabled: false,
        },
        lastHeartbeatMs: 32349024.02,
        numNodes: 0,
        numInstrumentedNodes: 4,
        controlPlanePodStatuses: [],
      },
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        clusterUID: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        clusterName: '789',
        prettyClusterName: 'pretty-789',
        status: GQLClusterStatus.CS_DISCONNECTED,
        clusterVersion: 'versionABC',
        vizierVersion: '0.2.4-pre-master.64',
        vizierConfig: {
          passthroughEnabled: false,
        },
        lastHeartbeatMs: 32349024.02,
        numNodes: 0,
        numInstrumentedNodes: 4,
        controlPlanePodStatuses: [],
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        clusterUID: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        clusterName: 'xyz',
        prettyClusterName: 'pretty-xyz',
        status: GQLClusterStatus.CS_UPDATE_FAILED,
        clusterVersion: 'versionDEF',
        vizierVersion: '0.2.4+Distribution.d98403c.20200515173726.1',
        vizierConfig: {
          passthroughEnabled: true,
        },
        lastHeartbeatMs: 24.92,
        numNodes: 8,
        numInstrumentedNodes: 8,
        controlPlanePodStatuses: [],
      },
    ];
    expect(formatClusters(clusterResults)).toStrictEqual([
      {
        clusterVersion: 'versionABC',
        lastHeartbeat: '8 hours 59 min 9 sec ago',
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        mode: 'Direct',
        name: '123',
        percentInstrumented: 'N/A (4 of 0)',
        percentInstrumentedLevel: 'low',
        prettyName: 'pretty-123',
        status: 'HEALTHY',
        statusGroup: 'healthy',
        vizierVersion: '0.2.4-pre-master.64',
        vizierVersionShort: '0.2.4-pre-master.64',
      },
      {
        clusterVersion: 'versionABC',
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        lastHeartbeat: '8 hours 59 min 9 sec ago',
        mode: 'Direct',
        name: '789',
        percentInstrumented: 'N/A',
        percentInstrumentedLevel: 'none',
        prettyName: 'pretty-789',
        status: 'DISCONNECTED',
        statusGroup: 'unknown',
        vizierVersion: '0.2.4-pre-master.64',
        vizierVersionShort: '0.2.4-pre-master.64',
      },
      {
        clusterVersion: 'versionDEF',
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        idShort: '10de7d77f1b2',
        lastHeartbeat: '0 sec ago',
        mode: 'Passthrough',
        name: 'xyz',
        percentInstrumented: '100% (8 of 8)',
        percentInstrumentedLevel: 'high',
        prettyName: 'pretty-xyz',
        status: 'UPDATE_FAILED',
        statusGroup: 'unhealthy',
        vizierVersion: '0.2.4+Distribution.d98403c.20200515173726.1',
        vizierVersionShort: '0.2.4',
      },
    ]);
  });
});
