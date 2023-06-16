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

import { TableContainer, Table, TableBody, TableRow } from '@mui/material';

import {
  MonoSpaceCell,
  ClusterStatusCell,
  InstrumentationLevelCell,
  VizierVersionCell,
} from 'app/containers/admin/cluster-table-cells';
import { convertHeartbeatMS, StyledTableCell } from 'app/containers/admin/utils';
import { GQLClusterInfo } from 'app/types/schema';

import { useClusterDetailStyles } from './cluster-details-utils';

export const ClusterSummaryTable = React.memo<{
  cluster: Pick<
  GQLClusterInfo,
  'clusterName' |
  'id' |
  'status' |
  'statusMessage' |
  'numNodes' |
  'numInstrumentedNodes' |
  'vizierVersion' |
  'operatorVersion' |
  'clusterVersion' |
  'lastHeartbeatMs'
  >
}>(({ cluster }) => {
  const classes = useClusterDetailStyles();
  if (!cluster) {
    return (
      <div>
        Cluster not found.
      </div>
    );
  }

  const data = [
    {
      key: 'Name',
      value: cluster.clusterName,
    },
    {
      key: 'ID',
      value: (<MonoSpaceCell data={cluster.id} />),
    },
    {
      key: 'Status',
      value: (<ClusterStatusCell status={cluster.status} message={cluster.statusMessage} />),
    },
    {
      key: 'Instrumented Nodes',
      value: (
        <InstrumentationLevelCell cluster={cluster} />
      ),
    },
    {
      key: 'Vizier Version',
      value: (<VizierVersionCell version={cluster.vizierVersion} />),
    },
    {
      key: 'Operator Version',
      value: (<VizierVersionCell version={cluster.operatorVersion} />),
    },
    {
      key: 'Kubernetes Version',
      value: cluster.clusterVersion,
    },
    {
      key: 'Heartbeat',
      value: convertHeartbeatMS(cluster.lastHeartbeatMs),
    },
  ];

  return (
    <TableContainer className={classes.tableContainer}>
      <div className={classes.detailsTable}>
        <Table>
          <TableBody>
            {data.map((r) => (
              <TableRow key={r.key}>
                <StyledTableCell >
                  {r.key}
                </StyledTableCell>
                {!React.isValidElement(r.value)
                  ? <StyledTableCell>{r.value}</StyledTableCell>
                  : r.value
                }
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </div >
    </TableContainer>
  );
});
ClusterSummaryTable.displayName = 'ClusterSummaryTable';
