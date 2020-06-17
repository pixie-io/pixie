import { matchPath } from 'react-router';

export enum LiveViewPage {
  Default,
  Namespace,
  Namespaces,
  Node,
  Nodes,
  Pod,
  Pods,
  Service,
  Services,
}

interface NamespaceURLParams {
  namespace: string;
}

interface NodeURLParams {
  node: string;
}

interface PodURLParams {
  pod: string;
}

interface ServiceURLParams {
  service: string;
}

export type EntityURLParams = {} | NamespaceURLParams | NodeURLParams | PodURLParams | ServiceURLParams;

export interface EntityURL {
  clusterName?: string;
  page: LiveViewPage;
  params: EntityURLParams;
}

export const LiveViewPageRoutes = {
  [LiveViewPage.Default]: '/live',
  [LiveViewPage.Namespace]: '/live/clusters/:cluster/namespaces/:namespace',
  [LiveViewPage.Namespaces]: '/live/clusters/:cluster/namespaces',
  [LiveViewPage.Node]: '/live/clusters/:cluster/nodes/:node',
  [LiveViewPage.Nodes]: '/live/clusters/:cluster/nodes',
  [LiveViewPage.Pod]: '/live/clusters/:cluster/namespaces/:namespace/pods/:pod',
  [LiveViewPage.Pods]: '/live/clusters/:cluster/namespaces/:namespace/pods',
  [LiveViewPage.Service]: '/live/clusters/:cluster/namespaces/:namespace/services/:service',
  [LiveViewPage.Services]: '/live/clusters/:cluster/namespaces/:namespace/services',
};

export const LiveViewPageScriptIds = {
  [LiveViewPage.Namespace]: 'px/namespace',
  [LiveViewPage.Namespaces]: 'px/namespaces',
  [LiveViewPage.Node]: 'px/node',
  [LiveViewPage.Nodes]: 'px/nodes',
  [LiveViewPage.Pod]: 'px/pod',
  [LiveViewPage.Pods]: 'px/pods',
  [LiveViewPage.Service]: 'px/service',
  [LiveViewPage.Services]: 'px/services',
};

interface WithCluster {
  cluster: string;
}

function matchAndExtractEntity<T>(path: string, page: LiveViewPage) {
  const match = matchPath<WithCluster & T>(path, {
    path: LiveViewPageRoutes[page],
    exact: true
  });
  if (!match) {
    return null;
  }
  const {cluster, ...params} = match.params as WithCluster & T;
  return {
    clusterName: decodeURIComponent(cluster),
    page,
    params,
  }
}

export function matchLiveViewEntity(path: string): EntityURL {
  // namespaces
  const namespaceMatch = matchAndExtractEntity<NamespaceURLParams>(
    path, LiveViewPage.Namespace
  );
  if (namespaceMatch) {
    return namespaceMatch;
  }
  const namespacesMatch = matchAndExtractEntity<{}>(
    path, LiveViewPage.Namespaces
  );
  if (namespacesMatch) {
    return namespacesMatch;
  }
  // nodes
  const nodeMatch = matchAndExtractEntity<NodeURLParams>(
    path, LiveViewPage.Node
  );
  if (nodeMatch) {
    return nodeMatch;
  }
  const nodesMatch = matchAndExtractEntity<{}>(
    path, LiveViewPage.Nodes
  );
  if (nodesMatch) {
    return nodesMatch;
  }
  // pods
  const podMatch = matchAndExtractEntity<NamespaceURLParams & PodURLParams>(
    path, LiveViewPage.Pod
  );
  if (podMatch) {
    return {
      ...podMatch,
      params: {
        // TODO(nserrino): remove this logic when we separate pod and namespace in the backend.
        pod: `${podMatch.params.namespace}/${podMatch.params.pod}`,
      },
    };
  }
  const podsMatch = matchAndExtractEntity<NamespaceURLParams>(
    path, LiveViewPage.Pods
  );
  if (podsMatch) {
    return podsMatch;
  }
  // services
  const serviceMatch = matchAndExtractEntity<NamespaceURLParams & ServiceURLParams>(
    path, LiveViewPage.Service
  );
  if (serviceMatch) {
    return {
      ...serviceMatch,
      params: {
        // TODO(nserrino): remove this logic when we separate service and namespace in the backend.
        service: `${serviceMatch.params.namespace}/${serviceMatch.params.service}`,
      },
    };
  }
  const servicesMatch = matchAndExtractEntity<NamespaceURLParams>(
    path, LiveViewPage.Services
  );
  if (servicesMatch) {
    return servicesMatch;
  }
  // non-exact cluster match.
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
    case LiveViewPage.Node: {
      const { node } = entity.params as NodeURLParams;
      return `/live/clusters/${encodedCluster}/nodes/${node}`;
    }
    case LiveViewPage.Nodes: {
      return `/live/clusters/${encodedCluster}/nodes`;
    }
    case LiveViewPage.Pod: {
      const { pod } = entity.params as PodURLParams;
      const [ namespace, podName ] = pod.split('/');
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/pods/${podName}`;
    }
    case LiveViewPage.Pods: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/pods`;
    }
    case LiveViewPage.Service: {
      const { service } = entity.params as ServiceURLParams;
      const [ namespace, serviceName ] = service.split('/');
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/services/${serviceName}`;
    }
    case LiveViewPage.Services: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/services`;
    }
    case LiveViewPage.Default:
    default:
      return `/live/clusters/${encodedCluster}`;
  }
}
