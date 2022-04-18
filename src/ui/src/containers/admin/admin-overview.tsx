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
import { PluginsOverview } from 'app/containers/admin/plugins/plugins-overview';
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

  const authClient = React.useMemo(() => GetOAuthProvider(), []);
  const showInvitationsTab = authClient.isInvitationEnabled();
  const InvitationTab: React.ComponentType = React.useMemo(
    // eslint-disable-next-line react-memo/require-memo
    () => (authClient.getInvitationComponent() ?? (() => (<>This tab should not have been reachable.</>))),
    [authClient]);

  const tabList: Array<{
    slug: string, label: string, component: React.ComponentType, enabled: boolean,
  }> = React.useMemo(() => [
    { slug: 'clusters',        label: 'Clusters',        component: ClustersTable,       enabled: true },
    { slug: 'deployment-keys', label: 'Deployment Keys', component: DeploymentKeysTable, enabled: true },
    { slug: 'api-keys',        label: 'API Keys',        component: APIKeysTable,        enabled: true },
    { slug: 'plugins',         label: 'Plugins',         component: PluginsOverview,     enabled: true },
    { slug: 'users',           label: 'Users',           component: UsersTable,          enabled: true },
    { slug: 'org',             label: 'Org Settings',    component: OrgSettings,         enabled: true },
    { slug: 'user',            label: 'User Settings',   component: UserSettings,        enabled: true },
    { slug: 'invite',          label: 'Invitations',     component: InvitationTab,       enabled: showInvitationsTab },
  ], [InvitationTab, showInvitationsTab]);

  // Example: extracts 'tabName' from '/admin/tabName/foo/bar' when 'path' is '/admin' or '/admin/'
  const pathTab: string = React.useMemo(() => {
    const fromPath = location.pathname.slice(path.length).split('/').filter((s: string) => s)[0].trim().toLowerCase();
    return tabList.find(t => t.enabled && t.slug === fromPath)?.slug ?? '';
  }, [tabList, location.pathname, path]);

  const [tab, setTab] = React.useState<string>(pathTab || 'clusters');

  const navTab = React.useCallback((newTab: string) => {
    setTab(newTab);
    history.push(`${path}/${newTab}`);
  }, [history, path]);

  React.useEffect(() => {
    if (pathTab === '') navTab('clusters');
    else if (tab !== pathTab) setTab(pathTab);
  }, [navTab, tab, pathTab]);

  const classes = useStyles();

  return (
    <div className={classes.tabRoot}>
      <div className={classes.tabBar}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <StyledTabs value={tab} onChange={(_, newTab) => navTab(newTab)}>
          {tabList.filter(t => t.enabled).map(({ slug, label }) => (
            <StyledTab key={slug} value={slug} label={label} />
          ))}
        </StyledTabs>
        {tab.endsWith('deployment-keys') && (
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
        {tab.endsWith('api-keys') && (
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
            {tabList.filter(t => t.enabled).map(({ slug, component }) => (
              <Route key={slug} path={`${path}/${slug}`} component={component} />
            ))}
          </Switch>
        </TableContainer>
      </div>
    </div>
  );
});
AdminOverview.displayName = 'AdminOverview';
