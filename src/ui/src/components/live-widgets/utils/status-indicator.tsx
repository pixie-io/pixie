// STATUS_TYPES contains types that should be displayed as a status indicator.
import * as React from 'react';
import Tooltip from '@material-ui/core/Tooltip';
import { StatusCell, StatusGroup } from '../../status/status';
import { SemanticType } from '../../../types/generated/vizier_pb';

export const STATUS_TYPES = new Set<SemanticType>([
  SemanticType.ST_POD_PHASE,
  SemanticType.ST_CONTAINER_STATE,
]);

function containerStateToStatusGroup(status: string): StatusGroup {
  switch (status) {
    case 'Running':
      return 'healthy';
    case 'Terminated':
      return 'unhealthy';
    case 'Waiting':
      return 'pending';
    case 'Unknown':
    default:
      return 'unknown';
  }
}

function podPhaseToStatusGroup(status: string): StatusGroup {
  switch (status) {
    case 'Running':
    case 'Succeeded':
      return 'healthy';
    case 'Failed':
      return 'unhealthy';
    case 'Pending':
      return 'pending';
    case 'Unknown':
    default:
      return 'unknown';
  }
}

export function toStatusIndicator(status: string, semanticType: SemanticType) {
  let statusGroup: StatusGroup;
  if (semanticType === SemanticType.ST_CONTAINER_STATE) {
    statusGroup = containerStateToStatusGroup(status);
  } else if (semanticType === SemanticType.ST_POD_PHASE) {
    statusGroup = podPhaseToStatusGroup(status);
  } else {
    return status;
  }
  return (
    <Tooltip title={status} interactive>
      <div>
        <StatusCell statusGroup={statusGroup}/>
      </div>
    </Tooltip>
  );
}
