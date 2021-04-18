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

import * as QueryString from 'query-string';
import { matchPath } from 'react-router';

import { SemanticType } from 'types/generated/vizierapi_pb';
import { Arguments } from 'utils/args-utils';

export enum LiveViewPage {
  Default,
  Cluster,
  Namespace,
  Namespaces,
  Node,
  Nodes,
  Pod,
  Pods,
  Service,
  Services,
}

// Use type not interface here so that these params are compatible with Arguments.
type NamespaceURLParams = {
  namespace: string;
};

type NodeURLParams = {
  node: string;
};

type PodURLParams = {
  pod: string;
};

type ServiceURLParams = {
  service: string;
};

export type EntityURLParams = {} | NamespaceURLParams | NodeURLParams | PodURLParams | ServiceURLParams;

export interface EntityPage {
  clusterName?: string;
  page: LiveViewPage;
  params: EntityURLParams;
}

// For live view entity routes.
export const LiveViewPageRoutes = new Map<LiveViewPage, string>([
  [LiveViewPage.Cluster, '/live/clusters/:cluster'],
  [LiveViewPage.Namespace, '/live/clusters/:cluster/namespaces/:namespace'],
  [LiveViewPage.Namespaces, '/live/clusters/:cluster/namespaces'],
  [LiveViewPage.Node, '/live/clusters/:cluster/nodes/:node'],
  [LiveViewPage.Nodes, '/live/clusters/:cluster/nodes'],
  [LiveViewPage.Pod, '/live/clusters/:cluster/namespaces/:namespace/pods/:pod'],
  [LiveViewPage.Pods, '/live/clusters/:cluster/namespaces/:namespace/pods'],
  [LiveViewPage.Service, '/live/clusters/:cluster/namespaces/:namespace/services/:service'],
  [LiveViewPage.Services, '/live/clusters/:cluster/namespaces/:namespace/services'],
]);

export const LiveViewPageScriptIds = new Map<LiveViewPage, string>([
  [LiveViewPage.Cluster, 'px/cluster'],
  [LiveViewPage.Namespace, 'px/namespace'],
  [LiveViewPage.Namespaces, 'px/namespaces'],
  [LiveViewPage.Node, 'px/node'],
  [LiveViewPage.Nodes, 'px/nodes'],
  [LiveViewPage.Pod, 'px/pod'],
  [LiveViewPage.Pods, 'px/pods'],
  [LiveViewPage.Service, 'px/service'],
  [LiveViewPage.Services, 'px/services'],
]);

export function entityPageForScriptId(id: string): LiveViewPage {
  for (const [page, scriptId] of LiveViewPageScriptIds) {
    if (scriptId === id) return page;
  }
  return LiveViewPage.Default;
}

// List of all of the keys of the entity arguments for a given live view page.
export const LiveViewEntityParams = new Map<LiveViewPage, Set<string>>([
  [LiveViewPage.Default, new Set()],
  [LiveViewPage.Cluster, new Set()],
  [LiveViewPage.Namespace, new Set(['namespace'])],
  [LiveViewPage.Namespaces, new Set()],
  [LiveViewPage.Node, new Set(['node'])],
  [LiveViewPage.Nodes, new Set()],
  [LiveViewPage.Pod, new Set(['pod'])],
  [LiveViewPage.Pods, new Set(['namespace'])],
  [LiveViewPage.Service, new Set(['service'])],
  [LiveViewPage.Services, new Set(['namespace'])],
]);

interface WithCluster {
  cluster: string;
}

function matchAndExtractEntity<T>(path: string, page: LiveViewPage) {
  const match = matchPath<WithCluster & T>(path, {
    path: LiveViewPageRoutes.get(page),
    exact: true,
  });
  if (!match) {
    return null;
  }
  const { cluster, ...params } = match.params as WithCluster & T;
  return {
    clusterName: decodeURIComponent(cluster),
    page,
    params,
  };
}

export function matchLiveViewEntity(path: string): EntityPage {
  // cluster
  const clusterMatch = matchAndExtractEntity<{}>(
    path, LiveViewPage.Cluster,
  );
  if (clusterMatch) {
    return clusterMatch;
  }

  // namespaces
  const namespaceMatch = matchAndExtractEntity<NamespaceURLParams>(
    path, LiveViewPage.Namespace,
  );
  if (namespaceMatch) {
    return namespaceMatch;
  }
  const namespacesMatch = matchAndExtractEntity<{}>(
    path, LiveViewPage.Namespaces,
  );
  if (namespacesMatch) {
    return namespacesMatch;
  }
  // nodes
  const nodeMatch = matchAndExtractEntity<NodeURLParams>(
    path, LiveViewPage.Node,
  );
  if (nodeMatch) {
    return nodeMatch;
  }
  const nodesMatch = matchAndExtractEntity<{}>(
    path, LiveViewPage.Nodes,
  );
  if (nodesMatch) {
    return nodesMatch;
  }
  // pods
  const podMatch = matchAndExtractEntity<NamespaceURLParams & PodURLParams>(
    path, LiveViewPage.Pod,
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
    path, LiveViewPage.Pods,
  );
  if (podsMatch) {
    return podsMatch;
  }
  // services
  const serviceMatch = matchAndExtractEntity<NamespaceURLParams & ServiceURLParams>(
    path, LiveViewPage.Service,
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
    path, LiveViewPage.Services,
  );
  if (servicesMatch) {
    return servicesMatch;
  }

  // cluster script match.
  const scriptMatch = matchPath<WithCluster>(path, {
    path: '/live/clusters/:cluster/script',
    exact: false,
  });
  if (scriptMatch) {
    return {
      clusterName: decodeURIComponent(scriptMatch.params.cluster),
      page: LiveViewPage.Default,
      params: {},
    };
  }

  return {
    page: LiveViewPage.Default,
    params: {},
  };
}

export function getLiveViewTitle(defaultTitle: string, page: LiveViewPage,
  params: EntityURLParams, clusterPrettyName: string): string {
  switch (page) {
    case LiveViewPage.Cluster: {
      return clusterPrettyName;
    }
    case LiveViewPage.Namespace: {
      const { namespace } = params as NamespaceURLParams;
      return `${namespace}` || defaultTitle;
    }
    case LiveViewPage.Namespaces: {
      return `${clusterPrettyName} namespaces`;
    }
    case LiveViewPage.Node: {
      const { node } = params as NodeURLParams;
      return `${node}` || defaultTitle;
    }
    case LiveViewPage.Nodes: {
      return `${clusterPrettyName} nodes`;
    }
    case LiveViewPage.Pod: {
      const { pod } = params as PodURLParams;
      return `${pod}` || defaultTitle;
    }
    case LiveViewPage.Pods: {
      const { namespace } = params as NamespaceURLParams;
      return `${namespace}/pods`;
    }
    case LiveViewPage.Service: {
      const { service } = params as ServiceURLParams;
      return `${service}` || defaultTitle;
    }
    case LiveViewPage.Services: {
      const { namespace } = params as NamespaceURLParams;
      return `${namespace}/services`;
    }
    case LiveViewPage.Default:
    default:
      return defaultTitle;
  }
}

export function toEntityPathname(entity: EntityPage): string {
  const encodedCluster = encodeURIComponent(entity.clusterName);
  switch (entity.page) {
    case LiveViewPage.Cluster: {
      return `/live/clusters/${encodedCluster}`;
    }
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
      const [namespace, podName] = (pod || 'unknown/unknown').split('/');
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/pods/${podName}`;
    }
    case LiveViewPage.Pods: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/pods`;
    }
    case LiveViewPage.Service: {
      const { service } = entity.params as ServiceURLParams;
      const [namespace, serviceName] = (service || 'unknown/unknown').split('/');
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/services/${serviceName}`;
    }
    case LiveViewPage.Services: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `/live/clusters/${encodedCluster}/namespaces/${namespace}/services`;
    }
    case LiveViewPage.Default:
    default:
      return `/live/clusters/${encodedCluster}/script`;
  }
}

export function toEntityURL(entity: EntityPage, propagatedArgs?: Arguments): string {
  const pathname = toEntityPathname(entity);
  let queryString = '';
  if (propagatedArgs) {
    queryString = QueryString.stringify(propagatedArgs);
  }
  return queryString ? `${pathname}?${queryString}` : pathname;
}

// Gets the arguments that are entity-specific (and should go in the URL path).
export function getEntityParams(liveViewPage: LiveViewPage, args: Arguments): EntityURLParams {
  const entityParamNames = LiveViewEntityParams.get(liveViewPage) || new Set();
  const entityParams = {};
  entityParamNames.forEach((paramName: string) => {
    if (args[paramName] != null) {
      entityParams[paramName] = args[paramName];
    }
  });
  return entityParams;
}

// Gets the arguments that are not entity-specific (and should go in the query string).
export function getNonEntityParams(liveViewPage: LiveViewPage, args: Arguments): Arguments {
  const entityParamNames = LiveViewEntityParams.get(liveViewPage) || new Set();
  const nonEntityParams = {};
  Object.keys(args).forEach((argName: string) => {
    if (!entityParamNames.has(argName)) {
      nonEntityParams[argName] = args[argName];
    }
  });
  return nonEntityParams;
}

// Takes a script and arguments and formats it as an entity URL if applicable.
export function scriptToEntityURL(script: string, clusterName: string, args: Arguments): string {
  const liveViewPage = entityPageForScriptId(script);
  const entityParams = getEntityParams(liveViewPage, args);
  const nonEntityParams = getNonEntityParams(liveViewPage, args);
  const entityPage = {
    clusterName,
    page: liveViewPage,
    params: entityParams,
  };
  return toEntityURL(entityPage, liveViewPage === LiveViewPage.Default ? {
    ...nonEntityParams,
    // non-entity pages require a script query parameter.
    script,
  } : nonEntityParams);
}

export function toSingleEntityPage(entityName: string, semanticType: SemanticType, clusterName: string): EntityPage {
  switch (semanticType) {
    case SemanticType.ST_SERVICE_NAME:
      return {
        clusterName,
        page: LiveViewPage.Service,
        params: {
          service: entityName,
        },
      };
    case SemanticType.ST_POD_NAME:
      return {
        clusterName,
        page: LiveViewPage.Pod,
        params: {
          pod: entityName,
        },
      };
    case SemanticType.ST_NODE_NAME:
      return {
        clusterName,
        page: LiveViewPage.Node,
        params: {
          node: entityName,
        },
      };
    case SemanticType.ST_NAMESPACE_NAME:
      return {
        clusterName,
        page: LiveViewPage.Namespace,
        params: {
          namespace: entityName,
        },
      };
    default:
      return null;
  }
}

// If we are in a pod or a service view, this function extracts the namespace
// from the entity params so that when we switch views, the namespace is maintained.
export function optionallyGetNamespace(args: Arguments): string {
  let valToSplit;
  if (args.pod) {
    valToSplit = args.pod;
  }
  if (args.service) {
    valToSplit = args.service;
  }
  if (valToSplit) {
    const split = valToSplit.split('/');
    if (split.length >= 2) {
      return split[0];
    }
  }
  return null;
}
