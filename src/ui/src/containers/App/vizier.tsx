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
import { VizierGRPCClientProvider } from 'common/vizier-grpc-client-context';
import { useSnackbar } from '@pixie-labs/components';
import AdminView from 'pages/admin/admin';
import CreditsView from 'pages/credits/credits';
import { SCRATCH_SCRIPT, ScriptsContextProvider } from 'containers/App/scripts-context';
import LiveView from 'pages/live/live';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';
import { generatePath } from 'react-router';
import * as QueryString from 'query-string';

import { makeStyles } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { useLDClient } from 'launchdarkly-react-client-sdk';
import { GQLClusterInfo, GQLClusterStatus, GQLUserInfo } from '@pixie-labs/api';
import { useQuery, gql } from '@apollo/client';

import { DeployInstructions } from './deploy-instructions';
import { selectClusterName } from './cluster-info';
import { RouteNotFound } from './route-not-found';
import { VizierRouteContext, VizierContextRouter } from './vizier-routing';

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

const ClusterWarningBanner: React.FC<{ user: Pick<GQLUserInfo, 'email' | 'orgName' > }> = ({ user }) => {
  const classes = useStyles();

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

// Convenience routes: sends `/scratch`, `/script/http_data`, and others to the appropriate Live url.
const ScriptShortcut = ({ match, location }) => {
  const { data } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'status'>[]
  }>(
    gql`
      query listClustersForConvenienceRoutes {
        clusters {
          clusterName
          status
        }
      }
    `,
    { pollInterval: 15000 },
  );

  const clusters = data?.clusters;
  const cluster = React.useMemo(() => selectClusterName(clusters ?? []), [clusters]);

  if (cluster == null) return null; // Wait for things to be ready

  let scriptId = '';
  if (match.params?.scriptId) {
    scriptId = `${match.params.orgId ?? 'px'}/${match.params.scriptId}`;
  }
  if (location.pathname === '/scratch' || location.pathname === '/scratchpad') {
    scriptId = SCRATCH_SCRIPT.id;
  }
  if (!scriptId) {
    return <Redirect to='/live' />;
  }

  const queryParams: Record<string, string> = { script: scriptId };
  const params = QueryString.stringify(queryParams);
  const newPath = generatePath(`/live/clusters/:cluster\\?${params}`, { cluster });

  return <Redirect to={newPath} />;
};

const Vizier = () => {
  const { data: countData, loading: countLoading } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'id'>[]
  }>(
    gql`
      query countClustersForDeployInstructions {
        clusters {
          id
        }
      }
    `,
    { pollInterval: 60000 },
  );
  const numClusters = countData?.clusters?.length ?? 0;

  const { selectedClusterID, selectedClusterStatus, selectedClusterVizierConfig } = React.useContext(ClusterContext);

  if (countLoading) { return <div>Loading...</div>; }

  if (numClusters === 0) {
    return <DeployInstructions />;
  }

  return (
    <VizierGRPCClientProvider
      clusterID={selectedClusterID}
      passthroughEnabled={selectedClusterVizierConfig?.passthroughEnabled}
      clusterStatus={selectedClusterStatus ?? GQLClusterStatus.CS_UNKNOWN}
    >
      <LiveView />
    </VizierGRPCClientProvider>
  );
};

type SelectedClusterInfo = Pick<GQLClusterInfo,
'id' | 'clusterName' | 'prettyClusterName' | 'clusterUID' | 'vizierConfig' | 'status'
>;

const invalidCluster = (name: string): SelectedClusterInfo => ({
  id: '',
  clusterUID: '',
  status: GQLClusterStatus.CS_UNKNOWN,
  vizierConfig: null,
  clusterName: name,
  prettyClusterName: name,
});

const ClusterContextProvider: React.FC = ({ children }) => {
  const showSnackbar = useSnackbar();

  const {
    scriptId, clusterName, args, push,
  } = React.useContext(VizierRouteContext);

  const { data, loading, error } = useQuery<{
    clusterByName: SelectedClusterInfo
  }>(
    gql`
      query selectedClusterInfo($name: String!) {
        clusterByName(name: $name) {
          id
          clusterName
          prettyClusterName
          clusterUID
          vizierConfig {
            passthroughEnabled
          }
          status
        }
      }
    `,
    { pollInterval: 60000, variables: { name: clusterName } },
  );

  const cluster = data?.clusterByName ?? invalidCluster(clusterName);

  const setClusterByName = React.useCallback((name: string) => {
    push(name, scriptId, args);
  }, [push, scriptId, args]);

  const clusterContext = React.useMemo(() => ({
    selectedClusterID: cluster?.id,
    selectedClusterName: cluster?.clusterName,
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterUID: cluster?.clusterUID,
    selectedClusterVizierConfig: cluster?.vizierConfig,
    selectedClusterStatus: cluster?.status,
    setClusterByName,
  }), [
    cluster,
    setClusterByName,
  ]);

  if (error?.message) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    // eslint-disable-next-line no-console
    console.error(error?.message);
  }

  if (loading) { return <div>Loading...</div>; }

  return (
    <ClusterContext.Provider value={clusterContext}>
      {children}
    </ClusterContext.Provider>
  );
};

const VizierWithProvider = () => (
  <ScriptsContextProvider>
    <VizierContextRouter>
      <ClusterContextProvider>
        <Vizier />
      </ClusterContextProvider>
    </VizierContextRouter>
  </ScriptsContextProvider>
);

export default function PixieWithContext(): React.ReactElement {
  const showSnackbar = useSnackbar();
  const { data, loading: loadingUser, error: userError } = useQuery<{
    user: Pick<GQLUserInfo, 'email' | 'orgName' >,
  }>(gql`
    query userInfoForContext{
      user {
        orgName
        email
      }
    }
  `);

  const user = data?.user;
  const userEmail = user?.email;
  const userOrg = user?.orgName;
  const ldClient = useLDClient();

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

  const errMsg = userError?.message;
  if (errMsg) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    // eslint-disable-next-line no-console
    console.error(errMsg);
  }

  if (loadingUser) { return <div>Loading...</div>; }
  return (
    <UserContext.Provider value={userContext}>
      <ClusterWarningBanner user={user} />
      <Switch>
        <Route path='/admin' component={AdminView} />
        <Route path='/credits' component={CreditsView} />
        <Route path='/live' component={VizierWithProvider} />
        <Route
          path={[
            '/script/:orgId/:scriptId',
            '/scripts/:orgId/:scriptId',
            '/s/:orgId/:scriptId',
            '/script/:scriptId',
            '/scripts/:scriptId',
            '/s/:scriptId',
            '/scratch',
            '/scratchpad',
          ]}
          component={ScriptShortcut}
        />
        <Redirect exact from='/' to='/live' />
        <Route path='/*' component={RouteNotFound} />
      </Switch>
    </UserContext.Provider>
  );
}
