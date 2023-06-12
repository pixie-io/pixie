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

import { useQuery, gql } from '@apollo/client';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useLDClient } from 'launchdarkly-react-client-sdk';
import * as QueryString from 'query-string';
import { generatePath } from 'react-router';
import { Redirect, Route, Switch } from 'react-router-dom';

import { ClusterContextProvider } from 'app/common/cluster-context';
import { isPixieEmbedded } from 'app/common/embed-context';
import OrgContext from 'app/common/org-context';
import UserContext from 'app/common/user-context';
import { useSnackbar } from 'app/components';
import { SCRATCH_SCRIPT, ScriptsContextProvider } from 'app/containers/App/scripts-context';
import AdminView from 'app/pages/admin/admin';
import { ConfigureDataExportView } from 'app/pages/configure-data-export/configure-data-export';
import LiveView from 'app/pages/live/live';
import { SetupRedirect, SetupView } from 'app/pages/setup/setup';
import {
  GQLClusterInfo,
  GQLUserInfo,
  GQLOrgInfo,
} from 'app/types/schema';

import { selectClusterName } from './cluster-info';
import { DeployInstructions } from './deploy-instructions';
import { LiveContextRouter } from './live-routing';
import { RouteNotFound } from './route-not-found';

const useStyles = makeStyles((theme: Theme) => createStyles({
  banner: {
    position: 'absolute',
    width: '100%',
    textAlign: 'center',
    top: 0,
    zIndex: theme.zIndex.appBar + 1,
    color: 'white',
    background: 'rgba(220,0,0,0.5)',
  },
}), { name: 'App' });

// eslint-disable-next-line react-memo/require-memo
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
ClusterWarningBanner.displayName = 'ClusterWarningBanner';

// Convenience routes: sends `/clusterID/:clusterID`,  to the appropriate Live url.
// eslint-disable-next-line react-memo/require-memo
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

  React.useEffect(() => {
    const cid = match.params?.clusterID;
    if (cid && !loading && error) {
      if (isPixieEmbedded()) {
        window.top.postMessage({
          error: {
            message: error.message,
            context: `Failed to find a name for cluster ID "${cid}", from the URL`,
          },
        }, '*');
      }
      // Dump to the crash screen
      throw new Error(`Failed to find a name for clusterID "${cid}"!\nMessage: ${error.message}\n`);
    }
  }, [match, loading, error]);

  if (cluster == null || loading || error) return null; // Wait for things to be ready

  // eslint-disable-next-line react-memo/require-usememo
  let path = generatePath('/live/clusters/:cluster', { cluster });
  if (location.search) {
    path += location.search;
  }

  return <Redirect to={path} />;
};
ClusterIDShortcut.displayName = 'ClusterIDShortcut';

// Convenience routes: sends `/scratch`, `/script/http_data`, and others to the appropriate Live url.
// eslint-disable-next-line react-memo/require-memo
const ScriptShortcut = ({ match, location }) => {
  const { data } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'status'>[]
  }>(
    gql`
      query listClustersForConvenienceRoutes {
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
  const newPath = generatePath<string>(`/live/clusters/:cluster\\?${params}`, { cluster });

  return <Redirect to={newPath} />;
};
ScriptShortcut.displayName = 'ScriptShortcut';

// eslint-disable-next-line react-memo/require-memo
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
Live.displayName = 'Live';

// eslint-disable-next-line react-memo/require-memo
const LiveWithProvider = () => (
  <ScriptsContextProvider>
    <LiveContextRouter>
      <ClusterContextProvider>
        <Live />
      </ClusterContextProvider>
    </LiveContextRouter>
  </ScriptsContextProvider>
);
LiveWithProvider.displayName = 'LiveWithProvider';

export default function PixieWithContext(): React.ReactElement {
  const showSnackbar = useSnackbar();
  const { data, loading: loadingUser, error: userError } = useQuery<{
    user: Pick<GQLUserInfo, 'id' | 'email' | 'orgName' >,
  }>(gql`
    query userInfoForContext{
      user {
        id
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

  const { data: orgData } = useQuery<{
    org: Pick<GQLOrgInfo, 'id' | 'name' | 'domainName' | 'idePaths'>
  }>(
    gql`
      query getOrgInfo {
        org {
          id
          name
          domainName
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
  const orgDomain = org?.domainName;
  const idePaths = org?.idePaths;

  // We check the orgName as valid here. orgData will still be returned even if the org doesn't
  // exist, but orgs must have a non-empty string name to be valid.
  const setupComplete = !!orgName;

  const orgContext = React.useMemo(() => ({
    org: {
      id: orgID,
      name: orgName,
      domainName: orgDomain,
      idePaths,
    },
  }), [orgID, orgName, orgDomain, idePaths]);
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
    // Wait one update cycle, since this can happen in the middle of updating other components in some cases
    setTimeout(() => showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 }));
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

  const returnUri = window.location.pathname.length > 1
    ? encodeURIComponent(window.location.pathname + window.location.search)
    : '';
  const setupRedirectUri = returnUri ? `/setup?redirect_uri=${returnUri}` : '/setup';

  if (loadingUser || !orgData) { return <div>Loading...</div>; }
  return (
    <UserContext.Provider value={userContext}>
      <OrgContext.Provider value={orgContext}>
        <ClusterWarningBanner user={user} />
        {
          setupComplete ? (
            <Switch>
              <Route path='/admin' component={AdminView} />
              <Route path={clusterPaths} component={ClusterIDShortcut} />
              <Route path='/live' component={LiveWithProvider} />
              <Route path='/embed/live' component={LiveWithProvider} />
              <Route path={scriptPaths} component={ScriptShortcut} />
              <Route path='/setup' component={SetupRedirect} />
              <Route path='/configure-data-export' component={ConfigureDataExportView} />
              <Route path='/embed/configure-data-export' component={ConfigureDataExportView} />
              <Redirect exact from='/' to='/live' />
              <Route path='/*' component={RouteNotFound} />
            </Switch>
          ) : (
            <Switch>
              <Route path='/setup' component={SetupView} />
              <Redirect from='/*' to={setupRedirectUri} />
            </Switch>
          )
        }
      </OrgContext.Provider>
    </UserContext.Provider>
  );
}
PixieWithContext.displayName = 'PixieWithContext';
