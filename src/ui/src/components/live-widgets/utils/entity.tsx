import { SemanticType } from '../../../types/generated/vizier_pb';
import { toEntityPathname, toSingleEntityPage } from './live-view-params';
import * as React from 'react';
import {Link} from 'react-router-dom';

export function isEntityType(semanticType: SemanticType): boolean {
  switch (semanticType) {
    case SemanticType.ST_SERVICE_NAME:
    case SemanticType.ST_POD_NAME:
    case SemanticType.ST_NODE_NAME:
    case SemanticType.ST_NAMESPACE_NAME:
      return true;
    default:
      return false;
  }
}

export function ToEntityLink(entity: string, semanticType: SemanticType, clusterName: string) {
  const page = toSingleEntityPage(entity, semanticType, clusterName);
  const pathname = toEntityPathname(page);
  return <Link to={pathname} className={'query-results--entity-link'}>{entity}</Link>;
}
