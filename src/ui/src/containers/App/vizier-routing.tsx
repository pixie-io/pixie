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
import * as QueryString from 'query-string';
import plHistory from 'utils/pl-history';
import { LocationDescriptorObject } from 'history';
import { SCRATCH_SCRIPT, ScriptsContext } from 'containers/App/scripts-context';
import { parseVis, Vis } from 'containers/live/vis';
import { RouteNotFound } from 'containers/App/route-not-found';
import { selectCluster } from 'containers/App/cluster-info';
import { useListClusters } from '@pixie-labs/api-react';
import { useSnackbar } from '@pixie-labs/components';

export interface VizierRouteContextProps {
  script: string;
  cluster: string;
  args: Record<string, string|string[]>;
  push: (script: string, args: Record<string, string|string[]>) => void;
  replace: (script: string, args: Record<string, string|string[]>) => void;
  routeFor: (script: string, args: Record<string, string|string[]>) => LocationDescriptorObject;
}

export const VizierRouteContext = React.createContext<VizierRouteContextProps>(null);

/** Some scripts have special mnemonic routes. They are vanity URLs for /clusters/:cluster/script?... and map as such */
const SCRIPT_ROUTES = new Map<string, string>([
  /* eslint-disable no-multi-spaces */
  ['px/cluster',      '/clusters/:cluster'],
  ['px/nodes',        '/clusters/:cluster/nodes'],
  ['px/node',         '/clusters/:cluster/nodes/:node'],
  ['px/namespaces',   '/clusters/:cluster/namespaces'],
  ['px/namespace',    '/clusters/:cluster/namespaces/:namespace'],
  ['px/pods',         '/clusters/:cluster/namespaces/:namespace/pods'],
  ['px/pod',          '/clusters/:cluster/namespaces/:namespace/pods/:pod'],
  ['px/services',     '/clusters/:cluster/namespaces/:namespace/services'],
  ['px/service',      '/clusters/:cluster/namespaces/:namespace/services/:service'],
  // TODO(nick,PC-917): Make sure this is still compatible with updated special logic for the scratch script.
  [SCRATCH_SCRIPT.id, '/clusters/:cluster/scratch'],
  /* eslint-enable no-multi-spaces */
]);

/**
 * Extracts the names of any unpopulated React Router route parameters, like ':a/:b?/c/d/:e' -> ['a', 'b', 'e']
 * If an empty array is returned, all parameters in the route have been populated.
 * Useful for checking that a route is fully specified before navigating to it.
 */
function extractUnmatchedRouteParams(route: string): string[] {
  const unmatchedParamFinder = /(^|\/):([a-zA-Z\d]+)\??(?=($|\/))/g; // Finds path segments that look like :foo or :foo?
  const unmatched: string[] = [];
  let match;
  do {
    match = unmatchedParamFinder.exec(route)?.[2];
    if (match) unmatched.push(match);
  } while (match);
  return unmatched;
}

/**
 * Checks the semantics of a route before visiting it. The following rules are checked:
 * - The cluster is specified, and Pixie is aware of it
 */
const useRouteValidityCheck = (clusterName: string): 'wait'|'valid'|'invalid' => {
  const [clusters, loadingClusters, clusterError] = useListClusters();

  const found: boolean = React.useMemo(
    () => !!(clusters || []).find((c) => c.clusterName === clusterName),
    [clusterName, clusters],
  );

  if (loadingClusters) return 'wait';
  if (clusterError || !found) return 'invalid';
  return 'valid';
};

const VizierRoute: React.FC<
Omit<VizierRouteContextProps, 'args'|'cluster'> & { defaultArgs: Record<string, string> }
> = ({
  script, defaultArgs, push, replace, routeFor, children,
}) => {
  const argsFromMatch = useParams<Record<string, string>>();
  const argsFromSearch = QueryString.parse(useLocation().search);
  const args: Record<string, string|string[]> = {
    ...defaultArgs,
    ...argsFromMatch,
    ...argsFromSearch,
  };

  // This is not a script arg, but it is used to run every script. Thus, it gets its own context property.
  // TODO(nick): Make cluster an arg to routeFor, push, and replace; ScriptContext to control via ClusterContext.
  //  Need to keep track of the selected cluster in ClusterContext, determine the default (first healthy atm) there too?
  const cluster: string = Array.isArray(args.cluster) ? args.cluster[0] : args.cluster;
  // delete args.cluster; // TODO(nick): Can't do this before updating params to routeFor and friends.

  // Sorting keys ensures that the stringified object looks the same regardless of the order of operations that built it
  const serializedArgs = JSON.stringify(args, Object.keys(args ?? {}).sort());
  const context: VizierRouteContextProps = React.useMemo(() => ({
    script, args, cluster, push, replace, routeFor,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [script, push, replace, serializedArgs]);

  // eslint-disable-next-line react-hooks/exhaustive-deps
  const expectedRoute = React.useMemo(() => routeFor(script, args), [script, routeFor, serializedArgs]);
  const actualRoute = useLocation();

  const showSnackbar = useSnackbar();
  const routeValidity = useRouteValidityCheck(cluster);

  // Only show this once per check.
  React.useEffect(() => {
    if (routeValidity === 'invalid') {
      showSnackbar?.({
        message: `Pixie can't find cluster "${cluster}". Please check spelling and whether Pixie can reach it.`,
        action: () => plHistory.push('/'),
        actionTitle: 'Home',
      });
    }
  }, [cluster, showSnackbar, routeValidity]);

  if (routeValidity === 'wait') {
    return null;
  }

  if (routeValidity === 'invalid') {
    return null;
  }

  // Consistency: ensure that the route matches the context. Same cluster, script, and args.
  if (actualRoute.search.slice(actualRoute.search.indexOf('?') + 1) !== expectedRoute.search) {
    return <Redirect to={expectedRoute} />;
  }

  return (
    <VizierRouteContext.Provider value={context}>{children}</VizierRouteContext.Provider>
  );
};

export const VizierContextRouter: React.FC = ({ children }) => {
  // TODO(nick): When upgrading React Router to >= v6, routes nest by default and the base is no longer needed here.
  const base = React.useMemo(() => '/live', []);

  const [clusters] = useListClusters();
  const defaultCluster = React.useMemo(() => selectCluster(clusters ?? []), [clusters])?.clusterName;

  const { scripts: availableScripts, loading: loadingAvailableScripts } = React.useContext(ScriptsContext);

  const defaultArgsForScript: (script: string) => Record<string, string|undefined> = React.useCallback((script) => {
    if (loadingAvailableScripts) return {};
    const vis: Vis = parseVis(availableScripts.get(script)?.vis ?? '');
    return (vis?.variables ?? []).reduce((a, c) => ({
      ...a,
      [c.name]: c.defaultValue,
    }), {} as Record<string, string|undefined>);
  }, [availableScripts, loadingAvailableScripts]);

  const routeFor = React.useCallback((script: string, args: Record<string, string>): LocationDescriptorObject => {
    const queryParams: Record<string, string> = {};
    let route = `${base}${SCRIPT_ROUTES.get(script)}`;
    if (!route) {
      route = `${base}${SCRIPT_ROUTES.get('px/cluster')}`;
      queryParams.script = script;
    }

    for (const [key, value] of Object.entries({ ...defaultArgsForScript(script), ...args })) {
      if (route.includes(`:${key}`)) {
        route = route.replace(new RegExp(`:${key}\\??`), value);
      } else {
        queryParams[key] = value;
      }
    }

    const unmatched = extractUnmatchedRouteParams(route);
    if (unmatched.length) {
      throw new Error(`routeFor(${script}, ${JSON.stringify(args)}) is missing arg(s): ${unmatched.join(', ')}`);
    }

    return { pathname: route, search: QueryString.stringify(queryParams) };
  }, [base, defaultArgsForScript]);

  const push: (script: string, args: Record<string, string>) => void = React.useMemo(() => (script, args) => {
    const route = routeFor(script, { ...defaultArgsForScript(script), ...args });
    if (route.pathname !== plHistory.location.pathname || route.search !== plHistory.location.search) {
      plHistory.push(route);
    }
  }, [defaultArgsForScript, routeFor]);

  const replace: (script: string, args: Record<string, string>) => void = React.useMemo(() => (script, args) => {
    const route = routeFor(script, { ...defaultArgsForScript(script), ...args });
    if (route.pathname !== plHistory.location.pathname || route.search !== plHistory.location.search) {
      plHistory.replace(route);
    }
  }, [defaultArgsForScript, routeFor]);

  if (defaultCluster == null) return null; // Wait for things to be ready

  return (
    <Switch>
      <Redirect exact from={base} to={`${base}/clusters/${defaultCluster}`} />
      {[...SCRIPT_ROUTES.entries()].map(([scriptId, route]) => (
        <Route
          exact
          key={scriptId}
          path={`${base}${route}`}
          render={() => (
            <VizierRoute
              script={scriptId}
              push={push}
              replace={replace}
              routeFor={routeFor}
              defaultArgs={defaultArgsForScript(scriptId)}
            >
              {children}
            </VizierRoute>
          )}
        />
      ))}
      <Route path={`${base}/*`} component={RouteNotFound} />
    </Switch>
  );
};
