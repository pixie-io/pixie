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

import { GQLClusterInfo as Cluster, GQLClusterStatus as ClusterStatus } from 'app/types/schema';

import { selectClusterName } from './cluster-info';

describe('selectCluster', () => {
  const commonClusterProps = {
    clusterVersion: '1.0.0',
    vizierVersion: '1.0.0',
    operatorVersion: '0.1.0',
    lastHeartbeatMs: 0,
    controlPlanePodStatuses: [],
    unhealthyDataPlanePodStatuses: [],
    numNodes: 1,
    numInstrumentedNodes: 1,
    statusMessage: 'test status',
  };

  it('should select the right default cluster', () => {
    const clusters: Cluster[] = [
      {
        id: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterUID: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterName: 'foobar1',
        prettyClusterName: 'Foobar 1',
        status: ClusterStatus.CS_DISCONNECTED,
        ...commonClusterProps,
      },
      {
        id: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar2',
        prettyClusterName: 'Foobar 2',
        status: ClusterStatus.CS_HEALTHY,
        ...commonClusterProps,
      },
      {
        id: '5fffeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: '5fffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar3',
        prettyClusterName: 'Foobar 3',
        status: ClusterStatus.CS_HEALTHY,
        ...commonClusterProps,
      },
    ];

    expect(selectClusterName(clusters)).toStrictEqual(clusters[1].clusterName);
  });

  it('should select the right default cluster with no ClusterStatus.CS_HEALTHY clusters', () => {
    const clusters: Cluster[] = [
      {
        id: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterUID: 'ca678ee7-55ea-4824-9623-e8159490b813',
        clusterName: 'foobar1',
        prettyClusterName: 'Foobar 1',
        status: ClusterStatus.CS_DISCONNECTED,
        ...commonClusterProps,
      },
      {
        id: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: 'efffeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar3',
        prettyClusterName: 'Foobar 3',
        status: ClusterStatus.CS_UPDATING,
        ...commonClusterProps,
      },
      {
        id: '6eefeb73-d19c-4630-af63-f75c3761bab4',
        clusterUID: '6eefeb73-d19c-4630-af63-f75c3761bab4',
        clusterName: 'foobar2',
        prettyClusterName: 'Foobar 2',
        status: ClusterStatus.CS_CONNECTED,
        ...commonClusterProps,
      },
    ];

    expect(selectClusterName(clusters)).toStrictEqual(clusters[2].clusterName);
  });
});
