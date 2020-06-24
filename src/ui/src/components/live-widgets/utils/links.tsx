import { SemanticType } from '../../../types/generated/vizier_pb';
import { toEntityPathname, toSingleEntityPage } from '../../../containers/live/utils/live-view-params';
import { Link } from 'react-router-dom';
import * as React from 'react';

export function ToEntityLink(entity: string, semanticType: SemanticType, clusterName: string) {
  const page = toSingleEntityPage(entity, semanticType, clusterName);
  const pathname = toEntityPathname(page);
  return <Link to={pathname} className={'query-results--entity-link'}>{entity}</Link>;
}
