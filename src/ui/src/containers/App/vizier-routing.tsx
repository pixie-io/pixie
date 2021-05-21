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
import {
  Switch, Route, Redirect, useParams, useLocation,
} from 'react-router-dom';
import { RouteChildrenProps, generatePath } from 'react-router';
import * as QueryString from 'query-string';
import plHistory from 'utils/pl-history';
import { LocationDescriptorObject } from 'history';
import { ClusterContext } from 'common/cluster-context';
import { SCRATCH_SCRIPT } from 'containers/App/scripts-context';
import { RouteNotFound } from 'containers/App/route-not-found';
import { selectCluster } from 'containers/App/cluster-info';
import { useListClusters } from '@pixie-labs/api-react';

export interface VizierRouteContextProps {
  scriptId: string;
  clusterName: string;
  args: Record<string, string|string[]>;
  push: (scriptId: string, args: Record<string, string|string[]>) => void;
  replace: (scriptId: string, args: Record<string, string|string[]>) => void;
  routeFor: (scriptId: string, args: Record<string, string|string[]>) => LocationDescriptorObject;
}

export const VizierRouteContext = React.createContext<VizierRouteContextProps>(null);

/** Some scripts have special mnemonic routes. They are vanity URLs for /clusters/:cluster?... and map as such */
const VANITY_ROUTES = new Map<string, string>([
  /* eslint-disable no-multi-spaces */
  ['/live/clusters/:cluster/nodes',                                   'px/nodes'],
  ['/live/clusters/:cluster/nodes/:node',                             'px/node'],
  ['/live/clusters/:cluster/namespaces',                              'px/namespaces'],
  ['/live/clusters/:cluster/namespaces/:namespace',                   'px/namespace'],
  ['/live/clusters/:cluster/namespaces/:namespace/pods',              'px/pods'],
  ['/live/clusters/:cluster/namespaces/:namespace/pods/:pod',         'px/pod'],
  ['/live/clusters/:cluster/namespaces/:namespace/services',          'px/services'],
  ['/live/clusters/:cluster/namespaces/:namespace/services/:service', 'px/service'],
  ['/live/clusters/:cluster/scratch',                                 SCRATCH_SCRIPT.id],
  /* eslint-enable no-multi-spaces */
]);

const VizierVanityRerouter = ({ match, location }: RouteChildrenProps) => {
  const scriptId = VANITY_ROUTES.get(match.path);
  const argsFromSearch = QueryString.parse(useLocation().search);
  const { cluster, ...argsFromMatch } = useParams<Record<string, string>>();

  if (scriptId === 'px/pod' && argsFromMatch.namespace != null && argsFromMatch.pod != null) {
    argsFromMatch.pod = `${argsFromMatch.namespace}/${argsFromMatch.pod}`;
    delete argsFromMatch.namespace;
  }
  if (scriptId === 'px/service' && argsFromMatch.namespace != null && argsFromMatch.service != null) {
    argsFromMatch.service = `${argsFromMatch.namespace}/${argsFromMatch.service}`;
    delete argsFromMatch.namespace;
  }
  const queryParams: Record<string, string | string[]> = {
    ...argsFromMatch,
    ...argsFromSearch,
    ...{ script: scriptId },
  };
  const params = QueryString.stringify(queryParams);
  const newPath = generatePath(`/live/clusters/:cluster\\?${params}`, { cluster });
  return <Redirect exact from={`${location.pathname}`} to={`${newPath}`} />;
};

const VizierRoute: React.FC< Omit<VizierRouteContextProps, 'args' | 'scriptId'> > = ({
  clusterName, push, replace, routeFor, children,
}) => {
  const { setClusterByName } = React.useContext(ClusterContext);

  const { script, ...args } = QueryString.parse(useLocation().search);
  const scriptId = Array.isArray(script) ? script[0] : (script ?? 'px/cluster');

  // Sorting keys ensures that the stringified object looks the same regardless of the order of operations that built it
  const serializedArgs = JSON.stringify(args, Object.keys(args ?? {}).sort());
  const context: VizierRouteContextProps = React.useMemo(() => ({
    scriptId, clusterName, args, push, replace, routeFor,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [scriptId, clusterName, serializedArgs, push, replace, routeFor, serializedArgs]);

  setClusterByName(clusterName);

  return (
    <VizierRouteContext.Provider value={context}>{children}</VizierRouteContext.Provider>
  );
};

export const VizierContextRouter: React.FC = ({ children }) => {
  const [clusters] = useListClusters();
  const defaultCluster = React.useMemo(() => selectCluster(clusters ?? []), [clusters])?.clusterName;
  const { selectedClusterName } = React.useContext(ClusterContext);

  const routeFor = React.useCallback((scriptId: string, args: Record<string, string>): LocationDescriptorObject => {
    const route = `/live/clusters/${selectedClusterName}`;
    const queryParams: Record<string, string> = {
      ...args,
      ...{ script: scriptId },
    };
    return { pathname: route, search: QueryString.stringify(queryParams) };
  }, [selectedClusterName]);

  const push: (scriptId: string, args: Record<string, string>) => void = React.useMemo(() => (scriptId, args) => {
    const route = routeFor(scriptId, args);
    if (route.pathname !== plHistory.location.pathname || route.search !== plHistory.location.search) {
      plHistory.push(route);
    }
  }, [routeFor]);

  const replace: (scriptId: string, args: Record<string, string>) => void = React.useMemo(() => (scriptId, args) => {
    const route = routeFor(scriptId, args);
    if (route.pathname !== plHistory.location.pathname || route.search !== plHistory.location.search) {
      plHistory.replace(route);
    }
  }, [routeFor]);

  if (defaultCluster == null) return null; // Wait for things to be ready

  return (
    <Switch>
      <Redirect exact from='/live' to={`/live/clusters/${defaultCluster}`} />
      {[...VANITY_ROUTES.keys()].map((route) => (
        <Route exact key={route} path={route} component={VizierVanityRerouter} />
      ))}
      <Route
        exact
        path='/live/clusters/:cluster'
        render={({ match, location }) => {
          let scriptId = QueryString.parse(location.search).script;
          if (Array.isArray(scriptId)) scriptId = scriptId[0];
          if (!scriptId) scriptId = 'px/cluster';
          return (
            <VizierRoute
              clusterName={match.params.cluster}
              push={push}
              replace={replace}
              routeFor={routeFor}
            >
              {children}
            </VizierRoute>
          );
        }}
      />
      <Route path='/live/*' component={RouteNotFound} />
    </Switch>
  );
};
