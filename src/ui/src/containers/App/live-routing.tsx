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

import * as React from 'react';

import { gql, useQuery } from '@apollo/client';
import * as QueryString from 'query-string';
import {
  Switch, Route, Redirect, useRouteMatch, useLocation,
} from 'react-router-dom';

import { EmbedContext, isPixieEmbedded } from 'app/common/embed-context';
import { selectClusterName } from 'app/containers/App/cluster-info';
import { RouteNotFound } from 'app/containers/App/route-not-found';
import { SCRATCH_SCRIPT, ScriptBundleErrorView, ScriptsContext } from 'app/containers/App/scripts-context';
import { EmbedState } from 'app/containers/live-widgets/utils/live-view-params';
import { GQLClusterInfo } from 'app/types/schema';
import { argsForVis, Arguments } from 'app/utils/args-utils';
import plHistory from 'app/utils/pl-history';
import { WithChildren } from 'app/utils/react-boilerplate';

export interface LiveRouteContextProps {
  clusterName: string;
  scriptId: string;
  args: Arguments;
  embedState: EmbedState;
}

export const LiveRouteContext = React.createContext<LiveRouteContextProps>(null);
LiveRouteContext.displayName = 'LiveRouteContext';

/** Some scripts have special mnemonic routes. They are vanity URLs for /clusters/:cluster?... and map as such */
const VANITY_ROUTES = new Map<string, string>([
  /* eslint-disable no-multi-spaces */
  ['/clusters',                                                  'px/cluster'],
  ['/clusters/:cluster',                                         'px/cluster'],
  ['/clusters/:cluster/nodes',                                   'px/nodes'],
  ['/clusters/:cluster/nodes/:node',                             'px/node'],
  ['/clusters/:cluster/namespaces',                              'px/namespaces'],
  ['/clusters/:cluster/namespaces/:namespace',                   'px/namespace'],
  ['/clusters/:cluster/namespaces/:namespace/pods',              'px/pods'],
  ['/clusters/:cluster/namespaces/:namespace/pods/:pod',         'px/pod'],
  ['/clusters/:cluster/namespaces/:namespace/services',          'px/services'],
  ['/clusters/:cluster/namespaces/:namespace/services/:service', 'px/service'],
  ['/clusters/:cluster/scratch',                                 SCRATCH_SCRIPT.id],
  // The bare live path will redirect to px/cluster but only if we have a cluster name available to pick.
  ['',                                                           'px/cluster'],
  /* eslint-enable no-multi-spaces */
]);

function unwrapQueryParam(param: string | string[]): string {
  return Array.isArray(param) ? param[0] : param;
}

function buildMatchParams(scriptId: string, routeParams: Record<string, string | string[]>) {
  const cluster = unwrapQueryParam(routeParams.cluster || '');
  const matchParams = { ...routeParams };
  delete matchParams.cluster;

  if (scriptId === 'px/pod' && matchParams.namespace != null && matchParams.pod != null) {
    matchParams.pod = `${matchParams.namespace}/${matchParams.pod}`;
    delete matchParams.namespace;
  }
  if (scriptId === 'px/service' && matchParams.namespace != null && matchParams.service != null) {
    matchParams.service = `${matchParams.namespace}/${matchParams.service}`;
    delete matchParams.namespace;
  }

  return { cluster, matchParams };
}

export function push(
  clusterName: string,
  scriptId: string,
  args: Arguments,
  embedState: EmbedState,
): void {
  const pathname = `/live/clusters/${encodeURIComponent(clusterName)}`;
  const queryParams: Arguments = {
    ...args,
    ...{ script: scriptId },
    ...(embedState.disableTimePicker ? {
      disable_time_picker: embedState.disableTimePicker.toString(),
    } : {}),
  };
  const search = `?${QueryString.stringify(queryParams)}`;
  if (pathname !== plHistory.location.pathname || search !== plHistory.location.search) {
    plHistory.push({ pathname, search });
  }
}

const LiveRoute: React.FC<WithChildren<LiveRouteContextProps>> = React.memo(({
  args, scriptId, embedState, clusterName, children,
}) => {
  const { timeArg } = React.useContext(EmbedContext);
  const copiedArgs = React.useMemo(() => {
    const copy = { ...args };
    if (timeArg && copy.start_time) {
      copy.start_time = timeArg;
    }
    return copy;
  }, [args, timeArg]);

  // Sorting keys ensures that the stringified object looks the same regardless of the order of operations that built it
  const serializedArgs = JSON.stringify(copiedArgs, Object.keys(copiedArgs ?? {}).sort());
  const context: LiveRouteContextProps = React.useMemo(() => ({
    scriptId, clusterName, embedState, args: copiedArgs,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [scriptId, clusterName, embedState, serializedArgs]);

  return (
    <LiveRouteContext.Provider value={context}>{children}</LiveRouteContext.Provider>
  );
});
LiveRoute.displayName = 'LiveRoute';

const VanityRouter: React.FC<WithChildren<{ outerPath: string }>> = React.memo(({ outerPath, children }) => {
  const location = useLocation();
  const { path, params: routeParams } = useRouteMatch();
  const nestedPath = path.substr(outerPath.length);

  const { data, loading: loadingCluster } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'status'>[]
  }>(
    gql`
      query listClustersForLiveViewRouting {
        clusters {
          id
          clusterName
          status
        }
      }
    `,
    // Other queries frequently update the cluster cache, so don't make excessive network calls.
    { pollInterval: 15000, fetchPolicy: 'cache-first' },
  );

  const clusters = data?.clusters;
  const defaultCluster = React.useMemo(() => selectClusterName(clusters ?? []), [clusters]);

  const {
    scripts: availableScripts,
    loading: loadingAvailableScripts,
    error: availableScriptsError,
  } = React.useContext(ScriptsContext);

  const isEmbedded = isPixieEmbedded();

  const {
    script: queryScriptId,
    widget: widgetName,
    disable_time_picker: disableTimePicker,
    ...queryParams
  } = React.useMemo(() => QueryString.parse(location.search), [location.search]);

  const scriptId = React.useMemo(() => {
    if (queryScriptId) return unwrapQueryParam(queryScriptId);
    return VANITY_ROUTES.get(nestedPath) ?? 'px/cluster';
  }, [nestedPath, queryScriptId]);

  const embedState: EmbedState = React.useMemo(() => {
    let disable = false;
    let widget = null;
    if (isEmbedded) {
      disable = unwrapQueryParam(disableTimePicker) === 'true';
      widget = unwrapQueryParam(widgetName);
    }
    return { isEmbedded, disableTimePicker: disable, widget };
  }, [isEmbedded, disableTimePicker, widgetName]);

  const { cluster, matchParams } = React.useMemo(
    () => buildMatchParams(scriptId, routeParams),
    [scriptId, routeParams]);

  const args: Arguments = React.useMemo(() => argsForVis(
    availableScripts.get(scriptId)?.vis, {
      ...matchParams,
      ...queryParams,
    }), [availableScripts, matchParams, queryParams, scriptId]);

  // Wait for things to be ready
  if (loadingCluster || loadingAvailableScripts) return null;

  if (availableScriptsError) return <ScriptBundleErrorView reason={availableScriptsError} />;

  // Special handling only if a default cluster is available and path is /live w/o args.
  // Otherwise we want to render the LiveRoute which eventually renders something helpful for new users.
  if (defaultCluster && ['', '/clusters', '/clusters/'].includes(nestedPath)) {
    return (<Redirect to={`${outerPath}/clusters/${encodeURIComponent(defaultCluster)}`} />);
  }

  return (
    <LiveRoute
      scriptId={scriptId}
      args={args}
      embedState={embedState}
      clusterName={decodeURIComponent(cluster)}
    >
      {children}
    </LiveRoute>
  );
});
VanityRouter.displayName = 'VanityRouter';

export const LiveContextRouter: React.FC<WithChildren> = React.memo(({ children }) => {
  const { path } = useRouteMatch();
  const vanities = React.useMemo(() => [...VANITY_ROUTES.keys()].map((r) => `${path}${r}`), [path]);

  return (
    <Switch>
      <Route exact path={vanities}>
        <VanityRouter outerPath={path}>{children}</VanityRouter>
      </Route>
      <Route path={`${path}*`} component={RouteNotFound} />
    </Switch>
  );
});
LiveContextRouter.displayName = 'LiveContextRouter';
