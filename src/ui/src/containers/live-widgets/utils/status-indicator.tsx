// STATUS_TYPES contains types that should be displayed as a status indicator.
import * as React from 'react';
import Tooltip from '@material-ui/core/Tooltip';
import { StatusCell, StatusGroup } from 'pixie-components';
import { SemanticType } from 'types/generated/vizierapi_pb';

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
    case 'Unknown':
    default:
      return 'unknown';
  }
}

export function toStatusIndicator(status: any, semanticType: SemanticType) {
  let statusGroup: StatusGroup = 'unknown';
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
    <Tooltip title={tooltipMsg} interactive>
      <div>
        <StatusCell statusGroup={statusGroup} />
      </div>
    </Tooltip>
  );
}
