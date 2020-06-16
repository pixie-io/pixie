import { matchPath } from 'react-router';

export enum LiveViewPage {
  Default,
  Namespaces
}

interface NamespaceURLParams {
  namespace: string;
}

export type EntityURLParams = {} | NamespaceURLParams;

export interface EntityURL {
  clusterName?: string;
  page: LiveViewPage;
  params: EntityURLParams;
}

export const LiveViewPageRoutes = {
  [LiveViewPage.Default]: '/live',
  [LiveViewPage.Namespaces]: '/live/clusters/:cluster/namespaces',
};

export const LiveViewPageScriptIds = {
  [LiveViewPage.Namespaces]: 'px/namespaces',
};

export function matchLiveViewEntity(path: string): EntityURL {
  const namespaceMatch = matchPath<{ cluster: string }>(path, {
    path: LiveViewPageRoutes[LiveViewPage.Namespaces],
    exact: true,
  });
  if (namespaceMatch) {
    return {
      clusterName: decodeURIComponent(namespaceMatch.params.cluster),
      page: LiveViewPage.Namespaces,
      params: {},
    }
  }

  const clusterMatch = matchPath<{ cluster: string }>(path, {
    path: '/live/clusters/:cluster',
    exact: false,
  });

  if (clusterMatch) {
    return {
      clusterName: decodeURIComponent(clusterMatch.params.cluster),
      page: LiveViewPage.Default,
      params: {},
    }
  }

  return {
    page: LiveViewPage.Default,
    params: {},
  };
}

export function toEntityPathname(entity: EntityURL): string {
  const encodedCluster = encodeURIComponent(entity.clusterName);
  switch (entity.page) {
    case LiveViewPage.Namespaces:
      return `/live/clusters/${encodedCluster}/namespaces`;
    case LiveViewPage.Default:
    default:
      return `/live/clusters/${encodedCluster}`;
  }
}
