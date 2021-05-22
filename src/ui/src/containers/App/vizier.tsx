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

import { ClusterContext } from 'common/cluster-context';
import UserContext from 'common/user-context';
import * as storage from 'common/storage';
import { VizierGRPCClientProvider } from 'common/vizier-grpc-client-context';
import { useSnackbar } from '@pixie-labs/components';
import AdminView from 'pages/admin/admin';
import CreditsView from 'pages/credits/credits';
import { SCRATCH_SCRIPT, ScriptsContextProvider } from 'containers/App/scripts-context';
import LiveView from 'pages/live/live';
import NewLiveView from 'pages/live/new-live';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';
import { useParams, useLocation } from 'react-router';
import * as QueryString from 'query-string';

import { makeStyles } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { useLDClient } from 'launchdarkly-react-client-sdk';
import { GQLClusterInfo as Cluster, GQLClusterStatus as ClusterStatus } from '@pixie-labs/api';
import { useListClusters, useListClustersVerbose, useUserInfo } from '@pixie-labs/api-react';
import { scriptToEntityURL } from 'containers/live-widgets/utils/live-view-params';
import { LIVE_VIEW_SCRIPT_ID_KEY, useSessionStorage } from 'common/storage';
import { DeployInstructions } from './deploy-instructions';
import { selectCluster } from './cluster-info';
import { RouteNotFound } from './route-not-found';

/*
 * TODO(nick,PC-917): Do not turn this on in a published diff until all of PC-917 is addressed by it.
 *  Upon doing so, remove the old paths entirely. Test _extremely_ thoroughly.
 */
const USE_SIMPLIFIED_ROUTING = false;

const useStyles = makeStyles(() => createStyles({
  banner: {
    position: 'absolute',
    width: '100%',
    textAlign: 'center',
    top: 0,
    zIndex: 1500, // TopBar has a z-index of 1300.
    color: 'white',
    background: 'rgba(220,0,0,0.5)',
  },
}));

const ClusterBanner = () => {
  const classes = useStyles();
  const [{ user }, loading, error] = useUserInfo();

  if (loading || error) {
    return null;
  }

  if (user?.email.split('@')[1] === 'pixie.support') {
    return (
      <div className={classes.banner}>
        {
          `You are viewing clusters for an external org: ${user.orgName}`
        }
      </div>
    );
  }

  return null;
};

const useSelectedCluster = () => {
  const [clusters, loading, error] = useListClustersVerbose();
  const { selectedCluster } = React.useContext(ClusterContext);
  const cluster = clusters?.find((c) => c.id === selectedCluster);
  return {
    loading, cluster, numClusters: clusters?.length ?? 0, error,
  };
};

// Convenience routes: sends `/scratch`, `/script/http_data`, and others to the appropriate Live url.
// eslint-disable-next-line react/require-default-props
const ScriptShortcut = ({ toNamespace = '', toScript = '' }: { toNamespace?: string; toScript?: string }) => {
  const { namespace, scriptId } = useParams<Record<string, string>>();
  let fullId = `${toNamespace || namespace || 'px'}/`;
  const normalized = (toScript || scriptId || '').trim().toLowerCase().replace(/[^a-z0-9]+/g, '-');
  if (['scratch', 'scratchpad', 'scratch-pad', 'scratch-script'].includes(normalized)) {
    fullId = SCRATCH_SCRIPT.id;
  } else {
    fullId += toScript || scriptId;
  }

  const { loading, cluster, error } = useSelectedCluster();

  const queryParams = QueryString.parse(useLocation().search);

  // The default /live route points to <Vizier /> where the actual error handling is.
  const destination = error || !cluster
    ? '/live'
    : scriptToEntityURL(fullId, cluster?.clusterName, queryParams);

  // ScriptContext reads session storage to find the currently selected script, and then redirects to it. This happens
  // AFTER the page finishes loading and rendering routes, which is a bug (PC-537) with no currently clear solution.
  // The workaround here, for now, is to simply overwrite that session storage before ScriptContext reads it.
  const [id, setId] = useSessionStorage(LIVE_VIEW_SCRIPT_ID_KEY, fullId);
  if (!error && !loading && cluster && fullId !== id) {
    setId(fullId);
  }

  return loading ? null : <Redirect to={destination} />;
};

// Selects a default cluster if one hasn't already been selected by the user.

const Vizier = () => {
  const showSnackbar = useSnackbar();

  const {
    loading: clusterLoading, error: clusterError, cluster, numClusters,
  } = useSelectedCluster();

  if (clusterLoading) { return <div>Loading...</div>; }

  const errMsg = clusterError?.message;
  if (errMsg) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    // eslint-disable-next-line no-console
    console.error(errMsg);
  }

  if (numClusters === 0) {
    return <DeployInstructions />;
  }

  const status: ClusterStatus = cluster?.status || ClusterStatus.CS_UNKNOWN;

  return (
    <VizierGRPCClientProvider
      clusterID={cluster.id}
      passthroughEnabled={cluster.vizierConfig.passthroughEnabled}
      clusterStatus={errMsg ? ClusterStatus.CS_UNKNOWN : status}
    >
      <ScriptsContextProvider>
        { USE_SIMPLIFIED_ROUTING ? <NewLiveView /> : <LiveView />}
      </ScriptsContextProvider>
    </VizierGRPCClientProvider>
  );
};

export default function WithClusterBanner(): React.ReactElement {
  const showSnackbar = useSnackbar();

  // USE_SIMPLIFIED_ROUTING is a compile-time const, so the rules of hooks are not violated - the order is consistent.
  const [clusterId, setClusterId] = USE_SIMPLIFIED_ROUTING
    // eslint-disable-next-line react-hooks/rules-of-hooks
    ? React.useState<string>('')
    : storage.useSessionStorage(storage.CLUSTER_ID_KEY, '');

  const [clusters, loadingClusters, clusterError] = useListClusters();
  const [{ user }, loadingUser, userError] = useUserInfo();

  const ldClient = useLDClient();

  const cluster: Cluster = (clusterId && clusters?.find((c) => c.id === clusterId)) || selectCluster(clusters ?? []);

  const clusterContext = React.useMemo(() => ({
    selectedCluster: cluster?.id,
    selectedClusterName: cluster?.clusterName,
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterUID: cluster?.clusterUID,
    setCluster: setClusterId,
    setClusterByName: (name: string) => {
      const foundId = name && clusters.find((c) => c.clusterName === name)?.id;
      const newClusterId = foundId || clusterId;
      setClusterId(newClusterId);
    },
  }), [clusterId, cluster?.clusterName, cluster?.prettyClusterName, cluster?.clusterUID, setClusterId, clusters]);

  const userEmail = user?.email;
  const userOrg = user?.orgName;

  const userContext = React.useMemo(() => ({
    user: {
      email: userEmail,
      orgName: userOrg,
    },
  }), [userEmail, userOrg]);

  React.useEffect(() => {
    if (ldClient != null && userEmail != null) {
      ldClient.identify({
        key: userEmail,
        email: userEmail,
        custom: {
          orgName: userOrg,
        },
      }).then();
    }
  }, [ldClient, userEmail, userOrg]);

  if (loadingClusters || loadingUser) { return <div>Loading...</div>; }

  const errMsg = clusterError?.message || userError?.message;
  if (errMsg) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    // eslint-disable-next-line no-console
    console.error(errMsg);
  }

  if (clusters && clusters.length > 0) {
    if (cluster?.id && clusterId !== cluster?.id) {
      setClusterId(cluster.id);
    }
  }

  return (
    <ClusterContext.Provider value={clusterContext}>
      <UserContext.Provider value={userContext}>
        <ClusterBanner />
        <Switch>
          <Route path='/admin' component={AdminView} />
          <Route path='/credits' component={CreditsView} />
          <Route path='/live' component={Vizier} />
          <Route
            path={[
              '/script/:namespace/:scriptId',
              '/scripts/:namespace/:scriptId',
              '/s/:namespace/:scriptId',
              '/script/:scriptId',
              '/scripts/:scriptId',
              '/s/:scriptId',
            ]}
            component={ScriptShortcut}
          />
          <Route path={['/scratch', '/scratchpad']} render={() => <ScriptShortcut toScript={SCRATCH_SCRIPT.id} />} />
          <Redirect exact from='/' to='/live' />
          <Route path='/*' component={RouteNotFound} />
        </Switch>
      </UserContext.Provider>
    </ClusterContext.Provider>
  );
}
