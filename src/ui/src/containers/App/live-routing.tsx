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

import { gql, useQuery } from '@apollo/client';
import * as React from 'react';
import {
  Switch, Route, Redirect, useRouteMatch,
} from 'react-router-dom';
import * as QueryString from 'query-string';

import { EmbedContext } from 'app/common/embed-context';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { RouteNotFound } from 'app/containers/App/route-not-found';
import { selectClusterName } from 'app/containers/App/cluster-info';
import { GQLClusterInfo } from 'app/types/schema';
import { argsForVis, Arguments } from 'app/utils/args-utils';
import { EmbedState } from 'app/containers/live-widgets/utils/live-view-params';
import plHistory from 'app/utils/pl-history';

export interface LiveRouteContextProps {
  clusterName: string;
  scriptId: string;
  args: Arguments;
  embedState: EmbedState;
  push: (clusterName: string, scriptId: string, args: Arguments, embedState: EmbedState) => void;
}

export const LiveRouteContext = React.createContext<LiveRouteContextProps>(null);

/** Some scripts have special mnemonic routes. They are vanity URLs for /clusters/:cluster?... and map as such */
const VANITY_ROUTES = new Map<string, string>([
  /* eslint-disable no-multi-spaces */
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
  // The bare live path will redirect to px/cluster but only if we have a clustername available to pick.
  ['',                                                           'px/cluster'],
  /* eslint-enable no-multi-spaces */
]);

const LiveRoute: React.FC<LiveRouteContextProps> = ({
  args, scriptId, embedState, clusterName, push, children,
}) => {
  const { timeArg } = React.useContext(EmbedContext);
  const copiedArgs = args;
  if (timeArg && copiedArgs.start_time) {
    copiedArgs.start_time = timeArg;
  }
  // Sorting keys ensures that the stringified object looks the same regardless of the order of operations that built it
  const serializedArgs = JSON.stringify(copiedArgs, Object.keys(copiedArgs ?? {}).sort());
  const context: LiveRouteContextProps = React.useMemo(() => ({
    scriptId, clusterName, embedState, args: copiedArgs, push,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [scriptId, clusterName, embedState, serializedArgs, push]);

  return (
    <LiveRouteContext.Provider value={context}>{children}</LiveRouteContext.Provider>
  );
};

const push = (
  clusterName: string,
  scriptId: string,
  args: Arguments,
  embedState: EmbedState,
) => {
  const pathname = `${embedState.isEmbedded ? '/embed' : ''}/live/clusters/${encodeURIComponent(clusterName)}`;
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
};

export const LiveContextRouter: React.FC = ({ children }) => {
  const { path } = useRouteMatch();

  const { data, loading: loadingCluster } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'status'>[]
  }>(
    gql`
      query listClustersForLiveViewRouting {
        clusters {
          clusterName
          status
        }
      }
    `,
    { pollInterval: 15000 },
  );

  const clusters = data?.clusters;
  const defaultCluster = React.useMemo(() => selectClusterName(clusters ?? []), [clusters]);

  const { scripts: availableScripts, loading: loadingAvailableScripts } = React.useContext(ScriptsContext);

  if (loadingCluster || loadingAvailableScripts) return null; // Wait for things to be ready

  return (
    <Switch>
      <Route
        exact
        path={[...Array.from(VANITY_ROUTES.keys()).map((r) => (`${path}${r}`))]}
        render={({ match, location }) => {
          const isEmbedded = match.path.startsWith('/embed');
          // Strip out the path matched by the router one level above us.
          const nestedPath = match.path.substr(path.length);
          // Special handling only if a default cluster is available and path is /live w/o args.
          // Otherwise we want to render the LiveRoute which eventually renders something helpful for new users.
          if (defaultCluster && nestedPath === '') {
            return (<Redirect to={`${path}/clusters/${encodeURIComponent(defaultCluster)}`} />);
          }
          const {
            script: queryScriptId,
            widget: widgetName,
            disable_time_picker: disableTimePicker,
            ...queryParams
          } = QueryString.parse(location.search);

          let scriptId = VANITY_ROUTES.get(nestedPath) ?? 'px/cluster';
          if (queryScriptId) {
            scriptId = Array.isArray(queryScriptId)
              ? queryScriptId[0]
              : queryScriptId;
          }
          const { cluster: matchedCluster, ...matchParams } = match.params;
          const cluster = matchedCluster || '';

          if (scriptId === 'px/pod' && matchParams.namespace != null && matchParams.pod != null) {
            matchParams.pod = `${matchParams.namespace}/${matchParams.pod}`;
            delete matchParams.namespace;
          }
          if (scriptId === 'px/service' && matchParams.namespace != null && matchParams.service != null) {
            matchParams.service = `${matchParams.namespace}/${matchParams.service}`;
            delete matchParams.namespace;
          }
          const args: Arguments = argsForVis(
            availableScripts.get(scriptId)?.vis, {
              ...matchParams,
              ...queryParams,
            });

          const embedState: EmbedState = {
            isEmbedded,
            disableTimePicker: false,
            widget: null,
          };
          if (isEmbedded) {
            embedState.disableTimePicker = (
              Array.isArray(disableTimePicker) ? disableTimePicker[0] : disableTimePicker
            ) === 'true';
            embedState.widget = Array.isArray(widgetName) ? widgetName[0] : widgetName;
          }

          return (
            <LiveRoute
              scriptId={scriptId}
              args={args}
              embedState={embedState}
              clusterName={decodeURIComponent(cluster)}
              push={push}
            >
              {children}
            </LiveRoute>
          );
        }}
      />
      <Route path={`${path}*`} component={RouteNotFound} />
    </Switch>
  );
};
