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

import { SemanticType } from 'app/types/generated/vizierapi_pb';
import { Arguments } from 'app/utils/args-utils';

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

export type EntityURLParams =
  | Record<string, string>
  | NamespaceURLParams
  | NodeURLParams
  | PodURLParams
  | ServiceURLParams;

export interface EntityPage {
  clusterName?: string;
  page: LiveViewPage;
  params: EntityURLParams;
}

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

export function toEntityPathname(entity: EntityPage, isEmbedded: boolean): string {
  const pathPrefix = isEmbedded ? '/embed/live' : '/live';

  const encodedCluster = encodeURIComponent(entity.clusterName);
  switch (entity.page) {
    case LiveViewPage.Cluster: {
      return `${pathPrefix}/clusters/${encodedCluster}`;
    }
    case LiveViewPage.Namespace: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `${pathPrefix}/clusters/${encodedCluster}/namespaces/${namespace}`;
    }
    case LiveViewPage.Namespaces: {
      return `${pathPrefix}/clusters/${encodedCluster}/namespaces`;
    }
    case LiveViewPage.Node: {
      const { node } = entity.params as NodeURLParams;
      return `${pathPrefix}/clusters/${encodedCluster}/nodes/${node}`;
    }
    case LiveViewPage.Nodes: {
      return `${pathPrefix}/clusters/${encodedCluster}/nodes`;
    }
    case LiveViewPage.Pod: {
      const { pod } = entity.params as PodURLParams;
      const [namespace, podName] = (pod || 'unknown/unknown').split('/');
      return `${pathPrefix}/clusters/${encodedCluster}/namespaces/${namespace}/pods/${podName}`;
    }
    case LiveViewPage.Pods: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `${pathPrefix}/clusters/${encodedCluster}/namespaces/${namespace}/pods`;
    }
    case LiveViewPage.Service: {
      const { service } = entity.params as ServiceURLParams;
      const [namespace, serviceName] = (service || 'unknown/unknown').split('/');
      return `${pathPrefix}/clusters/${encodedCluster}/namespaces/${namespace}/services/${serviceName}`;
    }
    case LiveViewPage.Services: {
      const { namespace } = entity.params as NamespaceURLParams;
      return `${pathPrefix}/clusters/${encodedCluster}/namespaces/${namespace}/services`;
    }
    case LiveViewPage.Default:
    default:
      return `${pathPrefix}/clusters/${encodedCluster}`;
  }
}

function getQueryParams(embedState: EmbedState, propagatedArgs?: Arguments) {
  const params = {
    ...propagatedArgs,
  };
  if (embedState.widget) {
    return {
      ...params,
      widget: embedState.widget,
    };
  }
  if (embedState.disableTimePicker) {
    return {
      ...params,
      disable_time_picker: 'true',
    };
  }
  return params;
}

export function toEntityURL(entity: EntityPage, embedState: EmbedState, propagatedArgs?: Arguments): string {
  const pathname = toEntityPathname(entity, embedState.isEmbedded);
  let queryString = '';
  if (propagatedArgs) {
    queryString = QueryString.stringify(getQueryParams(embedState, propagatedArgs));
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

// Specification taken from the URL for how the live view should be rendered.
export interface EmbedState {
  isEmbedded: boolean;
  disableTimePicker: boolean;
  widget: string | null;
}

// Takes a script and arguments and formats it as an entity URL if applicable.
export function scriptToEntityURL(script: string, clusterName: string, embedState: EmbedState,
  args: Arguments): string {
  const liveViewPage = entityPageForScriptId(script);
  const entityParams = getEntityParams(liveViewPage, args);
  const nonEntityParams = getNonEntityParams(liveViewPage, args);
  const entityPage = {
    clusterName,
    page: liveViewPage,
    params: entityParams,
  };
  return toEntityURL(entityPage, embedState, liveViewPage === LiveViewPage.Default ? {
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
