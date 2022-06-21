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

// Specification taken from the URL for how the live view should be rendered.
export interface EmbedState {
  disableTimePicker: boolean;
  widget: string | null;
}

// If the service is an arrary, return the first element. Otherwise, return
// the service.
const getFirstServiceIfArray = (service: string): string => {
  try {
    const parsedArray = JSON.parse(service);
    if (Array.isArray(parsedArray)) {
      if (parsedArray.length) {
        return parsedArray[0];
      }
    }
  } catch {
    // Do nothing.
  }
  return service;
};

// Returns the base URL prefix for all deep links (both vanity and non-vanity URLs).
const getClusterURLPrefix = (clusterName: string): string => {
  const encodedCluster = encodeURIComponent(clusterName);
  return `/live/clusters/${encodedCluster}`;
};

// Get the query string portion of the deep link from the input arguments.
const getQueryString = (embedState: EmbedState, args: Arguments, script?: string): string => {
  const params = {
    ...args,
  };
  if (script) {
    params.script = script;
  }
  if (embedState.widget) {
    params.widget = embedState.widget;
  }
  if (embedState.disableTimePicker) {
    params.disable_time_picker = 'true';
  }
  const result = QueryString.stringify(params);
  if (!result) {
    return '';
  }
  return `?${result}`;
};

type URLFormatter = (clusterName: string, embedState: EmbedState, args: Arguments,
  script?: string) => string;

// Creates the default deep link for a given input script (non-vanity URL).
const defaultURLFormatter: URLFormatter = (clusterName, embedState, args, script) => {
  const pathName = getClusterURLPrefix(clusterName);
  const queryString = getQueryString(embedState, args, script);
  return `${pathName}${queryString}`;
};

// Creates the vanity URL for px/cluster.
// Same thing as the default URL formatter, but with no script name passed in.
const clusterURLFormatter: URLFormatter = (clusterName, embedState, args) => defaultURLFormatter(
  clusterName, embedState, args);

// Helper function to reduce repetition when formatting vanity URLs.
const formatVanityURL = (clusterName: string, embedState: EmbedState, vanityPath: string,
  scriptArgs: Arguments): string => {
  const pathName = getClusterURLPrefix(clusterName);
  const queryString = getQueryString(embedState, scriptArgs);
  return `${pathName}/${vanityPath}${queryString}`;
};

// Creates the vanity URL for px/namespace.
const namespaceURLFormatter: URLFormatter = (clusterName, embedState, args) => {
  const { namespace, ...scriptArgs } = args;
  return formatVanityURL(clusterName, embedState, `namespaces/${namespace}`, scriptArgs);
};

// Creates the vanity URL for px/namespaces.
const namespacesURLFormatter: URLFormatter = (clusterName, embedState, args) => formatVanityURL(
  clusterName, embedState, 'namespaces', args);

// Creates the vanity URL for px/node.
const nodeURLFormatter: URLFormatter = (clusterName, embedState, args) => {
  const { node, ...scriptArgs } = args;
  return formatVanityURL(clusterName, embedState, `nodes/${node}`, scriptArgs);
};

// Creates the vanity URL for px/nodes.
const nodesURLFormatter: URLFormatter = (clusterName, embedState, args) => formatVanityURL(
  clusterName, embedState, 'nodes', args);

// Creates the vanity URL for px/pod.
const podURLFormatter: URLFormatter = (clusterName, embedState, args) => {
  const { pod, ...scriptArgs } = args;
  const [namespace, podName] = (pod as string || 'unknown/unknown').split('/');
  return formatVanityURL(clusterName, embedState, `namespaces/${namespace}/pods/${podName}`,
    scriptArgs);
};

// Creates the vanity URL for px/pods.
const podsURLFormatter: URLFormatter = (clusterName, embedState, args) => {
  const { namespace, ...scriptArgs } = args;
  return formatVanityURL(clusterName, embedState, `namespaces/${namespace}/pods`, scriptArgs);
};

// Creates the vanity URL for px/pod.
const serviceURLFormatter: URLFormatter = (clusterName, embedState, args) => {
  const { service, ...scriptArgs } = args;
  const [namespace, serviceName] = (getFirstServiceIfArray(service as string) || 'unknown/unknown').split('/');
  return formatVanityURL(clusterName, embedState, `namespaces/${namespace}/services/${serviceName}`,
    scriptArgs);
};

// Creates the vanity URL for px/pods.
const servicesURLFormatter: URLFormatter = (clusterName, embedState, args) => {
  const { namespace, ...scriptArgs } = args;
  return formatVanityURL(clusterName, embedState, `namespaces/${namespace}/services`, scriptArgs);
};

// URL formatters for any deep link that should be formatted differently than the
// default URL format.
const vanityURLFormatters = new Map<string, URLFormatter>([
  ['px/cluster', clusterURLFormatter],
  ['px/namespace', namespaceURLFormatter],
  ['px/namespaces', namespacesURLFormatter],
  ['px/node', nodeURLFormatter],
  ['px/nodes', nodesURLFormatter],
  ['px/pod', podURLFormatter],
  ['px/pods', podsURLFormatter],
  ['px/service', serviceURLFormatter],
  ['px/services', servicesURLFormatter],
]);

/**
 * Takes a script and arguments and returns the URL for the deep link to this script.
 * It formats it as an entity URL if applicable.
 */
export const deepLinkURLFromScript = (script: string, clusterName: string,
  embedState: EmbedState, args: Arguments): string => {
  let urlFormatter: URLFormatter;

  if (vanityURLFormatters.has(script)) {
    urlFormatter = vanityURLFormatters.get(script);
  } else {
    urlFormatter = defaultURLFormatter;
  }

  return urlFormatter(clusterName, embedState, args, script);
};

/**
 * Returns true if the input semantic type deep links.
 */
export function semanticTypeDeepLinks(semanticType: SemanticType): boolean {
  switch (semanticType) {
    case SemanticType.ST_IP_ADDRESS:
    case SemanticType.ST_NAMESPACE_NAME:
    case SemanticType.ST_NODE_NAME:
    case SemanticType.ST_POD_NAME:
    case SemanticType.ST_SERVICE_NAME:
      return true;
    default:
      return false;
  }
}

/**
 * Takes a value with a semantic type and generates a deep link if that semantic type
 * should be automatically deep linked. Returns null if there is no deep link for that
 * semantic type.
 */
export const deepLinkURLFromSemanticType = (semanticType: SemanticType, value: string,
  clusterName: string, embedState: EmbedState, propagatedArgs?: Arguments): string => {
  switch (semanticType) {
    case SemanticType.ST_IP_ADDRESS:
      return deepLinkURLFromScript('px/ip', clusterName, embedState, {
        ...propagatedArgs,
        ip: value,
      });
    case SemanticType.ST_POD_NAME:
      return deepLinkURLFromScript('px/pod', clusterName, embedState, {
        ...propagatedArgs,
        pod: value,
      });
    case SemanticType.ST_NAMESPACE_NAME:
      return deepLinkURLFromScript('px/namespace', clusterName, embedState, {
        ...propagatedArgs,
        namespace: value,
      });
    case SemanticType.ST_NODE_NAME:
      return deepLinkURLFromScript('px/node', clusterName, embedState, {
        ...propagatedArgs,
        node: value,
      });
    case SemanticType.ST_SERVICE_NAME:
      return deepLinkURLFromScript('px/service', clusterName, embedState, {
        ...propagatedArgs,
        service: value,
      });
    default:
      return null;
  }
};
