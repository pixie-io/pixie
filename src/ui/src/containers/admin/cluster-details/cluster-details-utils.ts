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

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { formatDistance } from 'date-fns';

import { StatusGroup } from 'app/components';
import { agentStatusGroup, containerStatusGroup, convertHeartbeatMS, podStatusGroup } from 'app/containers/admin/utils';
import {
  GQLPodStatus as PodStatus,
  GQLContainerStatus as ContainerStatus,
} from 'app/types/schema';

export enum AgentState {
  AGENT_STATE_UNKNOWN = 'AGENT_STATE_UNKNOWN',
  AGENT_STATE_HEALTHY = 'AGENT_STATE_HEALTHY',
  AGENT_STATE_UNRESPONSIVE = 'AGENT_STATE_UNRESPONSIVE',
  AGENT_STATE_DISCONNECTED = 'AGENT_STATE_DISCONNECTED',
}

export interface AgentInfo {
  agent_id: string;
  asid: number;
  hostname: string;
  ip_address: string;
  agent_state: AgentState;
  create_time: Date;
  last_heartbeat_ns: number;
}

export interface AgentDisplay {
  id: string;
  idShort: string;
  status: string;
  statusGroup: StatusGroup;
  hostname: string;
  lastHeartbeat: string;
  uptime: string;
}

export interface GroupedPodStatus extends Omit<PodStatus, 'containers'> {
  statusGroup: StatusGroup;
  containers: Array<ContainerStatus & { statusGroup: StatusGroup }>;
}

export function formatPodStatus(podStatus: PodStatus): GroupedPodStatus {
  return {
    ...podStatus,
    statusGroup: podStatusGroup(podStatus.status),
    containers: (podStatus.containers ?? []).map((container) => ({
      ...container,
      statusGroup: containerStatusGroup(container.state),
    })),
  };
}

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export function formatAgent(agentInfo: AgentInfo): AgentDisplay {
  const now = new Date();
  const agentID = agentInfo.agent_id;
  return {
    id: agentID,
    idShort: agentID.split('-').pop(),
    status: agentInfo.agent_state.replace('AGENT_STATE_', ''),
    statusGroup: agentStatusGroup(agentInfo.agent_state),
    hostname: agentInfo.hostname,
    lastHeartbeat: convertHeartbeatMS(agentInfo.last_heartbeat_ns / (1000 * 1000)),
    uptime: formatDistance(new Date(agentInfo.create_time), now, { addSuffix: false }),
  };
}

// combineReasonAndMessage returns a combination of a reason and a message. These fields are
// used by Kubernetes to detail state However, we don't need to separate them when we
// display to the user. We would rather combine them together.
export function combineReasonAndMessage(reason: string, message: string): string {
  // If both are defined we want to separate them with a semicolon
  if (message && reason) {
    return `${message}; ${reason}`;
  }
  return `${message}${reason}`;
}

export const useClusterDetailStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    position: 'relative',
    height: '100%',
    display: 'flex',
    flexFlow: 'column nowrap',
  },
  header: {
    flex: '0 0 auto',
    backgroundColor: theme.palette.background.four,
    height: theme.spacing(7),
    padding: theme.spacing(1.5),
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-between',
  },
  errorMessage: {
    ...theme.typography.body1,
    padding: theme.spacing(3),
  },
  tabHeader: {
    flex: '0 0 auto',
    position: 'relative',

    '&::after': {
      content: '""',
      position: 'absolute',
      zIndex: -1,
      bottom: 0,
      width: '100%',
      borderBottom: `1px solid ${theme.palette.background.five}`,
    },

    '& .MuiTabs-indicator': {
      height: '1px',
    },
  },
  tabContents: {
    flex: '1 1 auto',
    overflow: 'auto',
    margin: theme.spacing(1),
    display: 'flex',
    flexFlow: 'column nowrap',
  },
  tableContainer: {
    maxHeight: theme.spacing(100),
  },
  detailsTable: {
    maxWidth: theme.breakpoints.values.md,
  },
}), { name: 'ClusterDetails' });
