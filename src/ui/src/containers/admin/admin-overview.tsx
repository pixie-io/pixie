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

import { gql, useMutation } from '@apollo/client';
import { Add } from '@mui/icons-material';
import { Button, TableContainer } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import * as QueryString from 'query-string';
import {
  Route, Switch, useHistory, useLocation, useRouteMatch,
} from 'react-router-dom';

import { scrollbarStyles } from 'app/components';
import { APIKeysTable } from 'app/containers/admin/api-keys';
import { ClustersTable } from 'app/containers/admin/clusters-list';
import { DeploymentKeysTable } from 'app/containers/admin/deployment-keys';
import { InviteUserButton } from 'app/containers/admin/invite-user-button';
import { OrgSettings } from 'app/containers/admin/org-settings';
import { UsersTable } from 'app/containers/admin/org-users';
import { UserSettings } from 'app/containers/admin/user-settings';
import { StyledTab, StyledTabs } from 'app/containers/admin/utils';
import { GetOAuthProvider } from 'app/pages/auth/utils';
import { GQLAPIKeyMetadata, GQLDeploymentKeyMetadata } from 'app/types/schema';

const useStyles = makeStyles((theme: Theme) => createStyles({
  createButton: {
    margin: `${theme.spacing(0.75)} ${theme.spacing(3)}`,
    padding: `${theme.spacing(0.375)} ${theme.spacing(1.875)}`, // 3px 15px
  },
  tabRoot: {
    height: '100%',
    overflowY: 'auto',
    ...scrollbarStyles(theme),
  },
  tabBar: {
    position: 'sticky',
    top: 0,
    display: 'flex',
    zIndex: 1, // Without this, inputs and icons in the table layer on top and break the illusion.
    paddingBottom: theme.spacing(1), // Aligns the visual size of the tab bar with the margin above it.
    backgroundColor: theme.palette.background.paper,
  },
  tabContents: {
    margin: theme.spacing(1),
  },
  table: {
    paddingBottom: '1px', // Prevent an incorrect height calculation that shows a second scrollbar
  },
}), { name: 'AdminOverview' });

export const AdminOverview = React.memo(() => {
  const history = useHistory();
  const location = useLocation();
  const { path } = useRouteMatch();

  const showInviteDialog = QueryString.parse(location.search).invite === 'true';
  const onCloseInviteDialog = React.useCallback(() => {
    if (showInviteDialog) {
      // So that a page refresh after closing the dialog doesn't immediately open it again
      history.replace({ search: '' });
    }
  }, [history, showInviteDialog]);

  const [createAPIKey] = useMutation<{ CreateAPIKey: GQLAPIKeyMetadata }, void>(gql`
    mutation CreateAPIKeyFromAdminPage {
      CreateAPIKey {
        id
        desc
        createdAtMs
      }
    }
  `);
  const [createDeploymentKey] = useMutation<
  { CreateDeploymentKey: GQLDeploymentKeyMetadata }, void
  >(gql`
    mutation CreateDeploymentKeyFromAdminPage{
      CreateDeploymentKey {
        id
        desc
        createdAtMs
      }
    }
  `);

  // Path can be just /, or can be /trailing/slash. Thus variable length logic.
  const [tab, setTab] = React.useState(location.pathname.slice(path.length > 1 ? path.length + 1 : 0));

  function navTab(newTab: string) {
    setTab(newTab);
    history.push(newTab);
  }

  // In case one tab has a <Link> to another, update the tab bar.
  React.useEffect(() => {
    const pathTab = location.pathname.slice(path.length > 1 ? path.length + 1 : 0);
    if (['clusters', 'deployment-keys', 'api-keys', 'users', 'org', 'user'].includes(pathTab) && tab !== pathTab) {
      setTab(pathTab);
    }
  }, [tab, path, location.pathname]);

  const authClient = React.useMemo(() => GetOAuthProvider(), []);
  const InvitationTab = React.useMemo(
    // eslint-disable-next-line react-memo/require-memo
    () => (authClient.getInvitationComponent() ?? (() => (<>This tab should not have been reachable.</>))),
    [authClient]);

  const classes = useStyles();

  return (
    <div className={classes.tabRoot}>
      <div className={classes.tabBar}>
        <StyledTabs
          value={tab}
          // eslint-disable-next-line react-memo/require-usememo
          onChange={(event, newTab) => navTab(newTab)}
        >
          <StyledTab value='clusters' label='Clusters' />
          <StyledTab value='deployment-keys' label='Deployment Keys' />
          <StyledTab value='api-keys' label='API Keys' />
          <StyledTab value='users' label='Users' />
          <StyledTab value='org' label='Org Settings' />
          <StyledTab value='user' label='User Settings' />
          {authClient.isInvitationEnabled() && <StyledTab value='invite' label='Invitations' />}
        </StyledTabs>
        {tab.endsWith('deployment-keys')
          && (
            <Button
              // eslint-disable-next-line react-memo/require-usememo
              onClick={() => createDeploymentKey({
                // This immediately adds a row to the table, so gives the user
                // an indication that clicking "Add" did something but the data
                // added is "wrong" and flashes with the correct data once the
                // actual response comes in.
                // TODO: Maybe we should assign client side IDs here.
                // The key is hidden by default so that value changing on
                // server response isn't that bad.
                optimisticResponse: {
                  CreateDeploymentKey: {
                    id: '00000000-0000-0000-0000-000000000000',
                    desc: '',
                    createdAtMs: Date.now(),
                  },
                },
                update: (cache, { data }) => {
                  cache.modify({
                    fields: {
                      deploymentKeys: (existingKeys) => ([data.CreateDeploymentKey].concat(existingKeys)),
                    },
                  });
                },
              })}
              className={classes.createButton}
              variant='outlined'
              // eslint-disable-next-line react-memo/require-usememo
              startIcon={<Add />}
            >
              New key
            </Button>
          )}
        {tab.endsWith('api-keys')
          && (
            <Button
              // eslint-disable-next-line react-memo/require-usememo
              onClick={() => createAPIKey({
                // This immediately adds a row to the table, so gives the user
                // an indication that clicking "Add" did something but the data
                // added is "wrong" and flashes with the correct data once the
                // actual response comes in.
                // TODO: Maybe we should assign client side IDs here.
                // The key is hidden by default so that value changing on
                // server response isn't that bad.
                optimisticResponse: {
                  CreateAPIKey: {
                    id: '00000000-0000-0000-0000-000000000000',
                    desc: '',
                    createdAtMs: Date.now(),
                  },
                },
                update: (cache, { data }) => {
                  cache.modify({
                    fields: {
                      apiKeys: (existingKeys) => ([data.CreateAPIKey].concat(existingKeys)),
                    },
                  });
                },
              })}
              className={classes.createButton}
              variant='outlined'
              // eslint-disable-next-line react-memo/require-usememo
              startIcon={<Add />}
            >
              New key
            </Button>
          )}
        {tab.endsWith('users') && (
          <InviteUserButton
            className={classes.createButton}
            startOpen={showInviteDialog}
            onClose={onCloseInviteDialog}
          />
        )}
      </div>
      <div className={classes.tabContents}>
        <TableContainer className={classes.table}>
          <Switch>
            <Route exact path={`${path}/clusters`} component={ClustersTable} />
            <Route exact path={`${path}/deployment-keys`} component={DeploymentKeysTable} />
            <Route exact path={`${path}/api-keys`} component={APIKeysTable} />
            <Route exact path={`${path}/invite`} component={InvitationTab} />
            <Route exact path={`${path}/users`} component={UsersTable} />
            <Route exact path={`${path}/org`} component={OrgSettings} />
            <Route exact path={`${path}/user`} component={UserSettings} />
          </Switch>
        </TableContainer>
      </div>
    </div>
  );
});
AdminOverview.displayName = 'AdminOverview';
