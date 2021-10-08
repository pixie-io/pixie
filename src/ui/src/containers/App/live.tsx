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

import { ClusterContextProvider } from 'app/common/cluster-context';
import UserContext from 'app/common/user-context';
import OrgContext from 'app/common/org-context';
import { useSnackbar } from 'app/components';
import AdminView from 'app/pages/admin/admin';
import CreditsView from 'app/pages/credits/credits';
import { SCRATCH_SCRIPT, ScriptsContextProvider } from 'app/containers/App/scripts-context';
import LiveView from 'app/pages/live/live';
import pixieAnalytics from 'app/utils/analytics';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';
import { generatePath } from 'react-router';
import * as QueryString from 'query-string';

import { makeStyles } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { useLDClient } from 'launchdarkly-react-client-sdk';
import {
  GQLClusterInfo, GQLUserInfo, GQLUserSettings, GQLOrgInfo,
} from 'app/types/schema';
import { useQuery, gql } from '@apollo/client';

import { DeployInstructions } from './deploy-instructions';
import { selectClusterName } from './cluster-info';
import { RouteNotFound } from './route-not-found';
import { LiveContextRouter } from './live-routing';

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

// Convenience routes: sends `/clusterID/:clusterID`,  to the appropriate Live url.
const ClusterIDShortcut = ({ match, location }) => {
  const { data, loading, error } = useQuery<{
    cluster: Pick<GQLClusterInfo, 'clusterName'>
  }>(
    gql`
      query clusterNameByID($id: ID!) {
        cluster(id: $id) {
          clusterName
        }
      }
    `,
    { pollInterval: 2500, variables: { id: match.params?.clusterID } },
  );
  const cluster = data?.cluster.clusterName;
  if (cluster == null || loading || error) return null; // Wait for things to be ready

  let path = generatePath('/live/clusters/:cluster', { cluster });
  if (match.path.startsWith('/embed')) {
    path = `/embed${path}`;
  }
  if (location.search) {
    path += location.search;
  }

  return <Redirect to={path} />;
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
    // Other queries frequently update the cluster cache, so don't make excessive network calls.
    { pollInterval: 15000, fetchPolicy: 'cache-first' },
  );

  const clusters = data?.clusters;
  const cluster = React.useMemo(() => selectClusterName(clusters ?? []), [clusters]);

  if (cluster == null) return null; // Wait for things to be ready

  let scriptID = '';
  if (match.params?.scriptID) {
    scriptID = `${match.params.scriptNS ?? 'px'}/${match.params.scriptID}`;
  }
  if (location.pathname === '/scratch' || location.pathname === '/scratchpad') {
    scriptID = SCRATCH_SCRIPT.id;
  }
  if (!scriptID) {
    return <Redirect to='/live' />;
  }

  const queryParams: Record<string, string> = { script: scriptID };
  const params = QueryString.stringify(queryParams);
  const newPath = generatePath(`/live/clusters/:cluster\\?${params}`, { cluster });

  return <Redirect to={newPath} />;
};

const Live = () => {
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
    // Other queries frequently update the cluster cache, so don't make excessive network calls.
    { pollInterval: 60000, fetchPolicy: 'cache-first' },
  );
  const numClusters = countData?.clusters?.length ?? 0;

  if (countLoading) { return <div>Loading...</div>; }

  if (numClusters === 0) {
    return <DeployInstructions />;
  }

  return <LiveView />;
};

const LiveWithProvider = () => (
  <ScriptsContextProvider>
    <LiveContextRouter>
      <ClusterContextProvider>
        <Live />
      </ClusterContextProvider>
    </LiveContextRouter>
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

  // Load analytics for the user (if they haven't disabled analytics).
  const { data: userSettingsData } = useQuery<{ userSettings: GQLUserSettings }>(
    gql`
      query getSettingsForCurrentUser{
        userSettings {
          analyticsOptout
        }
      }
    `);

  const userSettings = userSettingsData?.userSettings;
  React.useEffect(() => {
    if (userSettings && !userSettings.analyticsOptout) {
      pixieAnalytics.enable();
      pixieAnalytics.load();
    } else {
      pixieAnalytics.disable();
    }
  }, [userSettings]);

  const userContext = React.useMemo(() => ({
    user: {
      email: userEmail,
      orgName: userOrg,
    },
  }), [userEmail, userOrg]);

  const { data: orgData } = useQuery<{
    org: Omit<GQLOrgInfo, 'enableApprovals'>
  }>(
    gql`
      query getOrgInfo {
        org {
          id
          name
          idePaths {
            IDEName
            path
          }
        }
      }
    `,
    // Ignore cache on first fetch, to pull in any IDE configs that may have changed.
    { fetchPolicy: 'network-only', nextFetchPolicy: 'cache-and-network' },
  );
  const org = orgData?.org;
  const orgID = org?.id;
  const orgName = org?.name;
  const idePaths = org?.idePaths;

  const orgContext = React.useMemo(() => ({
    org: {
      id: orgID,
      name: orgName,
      idePaths,
    },
  }), [orgID, orgName, idePaths]);
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

  const scriptPaths = React.useMemo(() => [
    '/script/:scriptNS/:scriptID',
    '/scripts/:scriptNS/:scriptID',
    '/s/:scriptNS/:scriptID',
    '/script/:scriptID',
    '/scripts/:scriptID',
    '/s/:scriptID',
    '/scratch',
    '/scratchpad',
  ], []);

  const clusterPaths = React.useMemo(() => [
    '/clusterID/:clusterID',
    '/embed/clusterID/:clusterID',
  ], []);

  if (loadingUser) { return <div>Loading...</div>; }
  return (
    <UserContext.Provider value={userContext}>
      <OrgContext.Provider value={orgContext}>
        <ClusterWarningBanner user={user} />
        <Switch>
          <Route path='/admin' component={AdminView} />
          <Route path='/credits' component={CreditsView} />
          <Route path={clusterPaths} component={ClusterIDShortcut} />
          <Route path='/live' component={LiveWithProvider} />
          <Route path='/embed/live' component={LiveWithProvider} />
          <Route path={scriptPaths} component={ScriptShortcut} />
          <Redirect exact from='/' to='/live' />
          <Route path='/*' component={RouteNotFound} />
        </Switch>
      </OrgContext.Provider>
    </UserContext.Provider>
  );
}
