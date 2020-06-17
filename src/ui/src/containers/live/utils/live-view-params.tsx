import { matchPath } from 'react-router';

export enum LiveViewPage {
  Default,
  Namespace,
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
  [LiveViewPage.Namespace]: '/live/clusters/:cluster/namespaces/:namespace',
  [LiveViewPage.Namespaces]: '/live/clusters/:cluster/namespaces',
};

export const LiveViewPageScriptIds = {
  [LiveViewPage.Namespace]: 'px/namespace',
  [LiveViewPage.Namespaces]: 'px/namespaces',
};

interface WithCluster {
  cluster: string;
}

export function matchLiveViewEntity(path: string): EntityURL {
  const namespacesMatch = matchPath<WithCluster>(path, {
    path: LiveViewPageRoutes[LiveViewPage.Namespaces],
    exact: true,
  });
  if (namespacesMatch) {
    return {
      clusterName: decodeURIComponent(namespacesMatch.params.cluster),
      page: LiveViewPage.Namespaces,
      params: {},
    }
  }

  const namespaceMatch = matchPath<WithCluster & NamespaceURLParams>(path, {
    path: LiveViewPageRoutes[LiveViewPage.Namespace],
    exact: true,
  });
  if (namespaceMatch) {
    return {
      clusterName: decodeURIComponent(namespaceMatch.params.cluster),
      page: LiveViewPage.Namespace,
      params: {
        namespace: namespaceMatch.params.namespace,
      },
    }
  }

  const clusterMatch = matchPath<WithCluster>(path, {
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
    case LiveViewPage.Namespace: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}`;
    }
    case LiveViewPage.Namespaces: {
      return `/live/clusters/${encodedCluster}/namespaces`;
    }
    case LiveViewPage.Default:
    default:
      return `/live/clusters/${encodedCluster}`;
  }
}
