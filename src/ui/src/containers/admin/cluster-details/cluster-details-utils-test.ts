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

import { type AgentInfo, AgentState, formatAgent } from './cluster-details-utils';

describe('formatAgent', () => {
  it('correctly formats agent info', () => {
    const agentResults: AgentInfo[] = [
      {
        agent_id: '00000000-0000-006f-0000-0000000000de',
        asid: 1780,
        hostname: 'gke-host',
        ip_address: '',
        agent_state: AgentState.AGENT_STATE_HEALTHY,
        create_time: new Date(new Date().getTime() - 1000 * 60 * 60 * 24 * 2), // 2 days ago
        last_heartbeat_ns: 100074517116,
      },
      {
        agent_id: '00000000-0000-014d-0000-0000000001bc',
        asid: 1780,
        hostname: 'gke-host2',
        ip_address: '',
        agent_state: AgentState.AGENT_STATE_UNKNOWN,
        create_time: new Date(new Date().getTime() - 1000 * 60 * 60 * 3), // 3 hours ago
        last_heartbeat_ns: 1574517116,
      },
    ];
    expect(agentResults.map((agent) => formatAgent(agent))).toStrictEqual([
      {
        id: '00000000-0000-006f-0000-0000000000de',
        idShort: '0000000000de',
        status: 'HEALTHY',
        statusGroup: 'healthy',
        hostname: 'gke-host',
        lastHeartbeat: '1 min 40 sec ago',
        uptime: '2 days',
      },
      {
        id: '00000000-0000-014d-0000-0000000001bc',
        idShort: '0000000001bc',
        status: 'UNKNOWN',
        statusGroup: 'unknown',
        hostname: 'gke-host2',
        lastHeartbeat: '1 sec ago',
        uptime: 'about 3 hours',
      },
    ]);
  });
});
