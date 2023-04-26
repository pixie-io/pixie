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

// STATUS_TYPES contains types that should be displayed as a status indicator.
import * as React from 'react';

import { Tooltip } from '@mui/material';

import { StatusCell, StatusGroup } from 'app/components';
import { SemanticType } from 'app/types/generated/vizierapi_pb';

export const STATUS_TYPES = new Set<SemanticType>([
  SemanticType.ST_CONTAINER_STATE,
  SemanticType.ST_CONTAINER_STATUS,
  SemanticType.ST_POD_PHASE,
  SemanticType.ST_POD_STATUS,
]);

function containerStateToStatusGroup(status: string, reason?: string): StatusGroup {
  switch (status) {
    case 'Running':
      return 'healthy';
    case 'Terminated':
      if (reason === '') {
        return 'healthy';
      }
      return 'unhealthy';
    case 'Waiting':
      return 'pending';
    case 'Unknown':
    default:
      return 'unknown';
  }
}

function podPhaseToStatusGroup(status: string, ready: boolean): StatusGroup {
  switch (status) {
    case 'Running':
    case 'Succeeded':
      return (ready == null || ready) ? 'healthy' : 'pending';
    case 'Failed':
      return 'unhealthy';
    case 'Pending':
      return 'pending';
    case 'Terminated':
    case 'Unknown':
    default:
      return 'unknown';
  }
}

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export function toStatusIndicator(status: any, semanticType: SemanticType) {
  // eslint-disable-next-line react-memo/require-usememo
  let statusGroup: StatusGroup = 'unknown';
  // eslint-disable-next-line react-memo/require-usememo
  let tooltipMsg: string = typeof status === 'string' ? status : JSON.stringify(status);

  switch (semanticType) {
    case SemanticType.ST_CONTAINER_STATE: {
      statusGroup = containerStateToStatusGroup(status);
      break;
    }
    case SemanticType.ST_CONTAINER_STATUS: {
      if (status != null) {
        const { state, reason, message } = status;
        if (state != null && reason != null && message != null) {
          statusGroup = containerStateToStatusGroup(state, reason);
          tooltipMsg = `State: ${state}. Message: ${message || '<none>'}. `
            + `Reason: ${reason || '<none>'}`;
        }
      }
      break;
    }
    case SemanticType.ST_POD_PHASE: {
      statusGroup = podPhaseToStatusGroup(status, true);
      break;
    }
    case SemanticType.ST_POD_STATUS: {
      if (status != null) {
        const {
          phase, reason, message, ready,
        } = status;
        if (phase != null && reason != null && message != null) {
          statusGroup = podPhaseToStatusGroup(phase, ready);
          tooltipMsg = `Phase: ${phase}. Message: ${message || '<none>'}. `
            + `Reason: ${reason || '<none>'}. Ready: ${ready || '<none>'}.`;
        }
      }
      break;
    }
    default:
      return status;
  }

  return (
    <Tooltip title={tooltipMsg}>
      <div style={{ textAlign: 'center' }}>
        <StatusCell statusGroup={statusGroup} />
      </div>
    </Tooltip>
  );
}
