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

import { Table, TableHead, TableRow, TableBody, TableContainer } from '@mui/material';
import { BehaviorSubject } from 'rxjs';
import { filter, tap } from 'rxjs/operators';

import { PixieAPIContext, ExecutionStateUpdate, VizierQueryResult } from 'app/api';
import { useClusterConfig } from 'app/common/cluster-context';
import { StatusCell } from 'app/components';
import {
  StyledTableHeaderCell,
  AdminTooltip,
  StyledLeftTableCell,
  StyledTableCell,
  StyledRightTableCell,
  clusterStatusGroup,
} from 'app/containers/admin/utils';
import { GQLClusterInfo } from 'app/types/schema';
import { dataFromProto } from 'app/utils/result-data-utils';

import { AgentInfo, formatAgent, useClusterDetailStyles } from './cluster-details-utils';

const AGENT_STATUS_SCRIPT = `import px
px.display(px.GetAgentStatus())`;

const AGENTS_POLL_INTERVAL = 2500;

interface AgentDisplayState {
  error?: string;
  data: Array<AgentInfo>;
}

const AgentsTableContent = React.memo<{ agents: AgentInfo[] }>(({ agents }) => {
  const agentsDisplay = agents.map((agent) => formatAgent(agent));
  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell />
          <StyledTableHeaderCell>ID</StyledTableHeaderCell>
          <StyledTableHeaderCell>Hostname</StyledTableHeaderCell>
          <StyledTableHeaderCell>Last Heartbeat</StyledTableHeaderCell>
          <StyledTableHeaderCell>Uptime</StyledTableHeaderCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {agentsDisplay.map((agent) => (
          <TableRow key={agent.id}>
            <AdminTooltip title={agent.status}>
              <StyledLeftTableCell>
                <StatusCell statusGroup={agent.statusGroup} />
              </StyledLeftTableCell>
            </AdminTooltip>
            <AdminTooltip title={agent.id}>
              <StyledTableCell>{agent.idShort}</StyledTableCell>
            </AdminTooltip>
            <StyledTableCell>{agent.hostname}</StyledTableCell>
            <StyledTableCell>{agent.lastHeartbeat}</StyledTableCell>
            <StyledRightTableCell>{agent.uptime}</StyledRightTableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );
});
AgentsTableContent.displayName = 'AgentsTableContent';

const AgentsTable = React.memo(() => {
  const clusterConfig = useClusterConfig();
  const client = React.useContext(PixieAPIContext);

  const [state, setState] = React.useState<AgentDisplayState>({ data: [] });

  React.useEffect(() => {
    if (!client) {
      return () => {
      }; // noop
    }
    const executionSubject = new BehaviorSubject<ExecutionStateUpdate | null>(null);
    const fetchAgentStatus = () => {
      const onResults = (results: VizierQueryResult) => {
        if (!results.schemaOnly) {
          if (results.tables.length !== 1) {
            if (results.status) {
              setState({ data: [], error: results.status.getMessage() });
            }
            return;
          }
          const data = dataFromProto(results.tables[0].relation, results.tables[0].batches);
          setState({ data });
        }
      };
      const onError = (error) => {
        setState({ data: [], error: error?.message });
      };
      client.executeScript(clusterConfig, AGENT_STATUS_SCRIPT, { enableE2EEncryption: true }, []).pipe(
        filter((update) => !['data', 'cancel', 'error'].includes(update.event.type)),
        tap((update) => {
          if (update.event.type === 'error') {
            onError(update.event.error);
          } else {
            onResults(update.results);
          }
        }),
      ).subscribe(executionSubject);
    };

    fetchAgentStatus(); // Fetch the agent status initially, before starting the timer for AGENTS_POLL_INTERVAL.
    const interval = setInterval(() => {
      fetchAgentStatus();
    }, AGENTS_POLL_INTERVAL);
    return () => {
      clearInterval(interval);
      if (executionSubject.value?.cancel) {
        executionSubject.value?.cancel();
      }
      executionSubject.unsubscribe();
    };
  }, [client, clusterConfig]);

  if (state.error) {
    return (
      <span>
        Error!
        {state.error}
      </span>
    );
  }
  return <AgentsTableContent agents={state.data} />;
});
AgentsTable.displayName = 'AgentsTable';

export const AgentsTab = React.memo<{
  cluster: Pick<GQLClusterInfo, 'id' | 'clusterName' | 'status' | 'unhealthyDataPlanePodStatuses'>
}>(({ cluster }) => {
  const classes = useClusterDetailStyles();
  const statusGroup = clusterStatusGroup(cluster.status);

  return (
    <>
      <TableContainer className={classes.tableContainer}>
        {(statusGroup !== 'healthy' && statusGroup !== 'degraded') ? (
          <div className={classes.errorMessage}>
            {`Cannot get agents for cluster ${cluster.clusterName}, reason: ${statusGroup}`}
          </div>
        )
          : (<AgentsTable />)
        }
      </TableContainer>
    </>
  );
});
AgentsTab.displayName = 'AgentsTab';
