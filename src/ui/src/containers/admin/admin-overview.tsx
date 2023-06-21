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

import { TableContainer } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import {
  Route, Switch, useHistory, useLocation, useRouteMatch,
} from 'react-router-dom';

import { scrollbarStyles } from 'app/components';
import { ClustersTable } from 'app/containers/admin/clusters-list';
import { OrgSettings } from 'app/containers/admin/org-settings';
import { UsersTable } from 'app/containers/admin/org-users';
import { PluginsOverview } from 'app/containers/admin/plugins/plugins-overview';
import { UserSettings } from 'app/containers/admin/user-settings';
import { GetOAuthProvider } from 'app/pages/auth/utils';

import { KeysOverview } from './keys-overview';

const useStyles = makeStyles((theme: Theme) => createStyles({
  tabRoot: {
    height: '100%',
    overflowY: 'auto',
    ...scrollbarStyles(theme),
  },
  tabContents: {
    margin: theme.spacing(1),
    minHeight: `calc(100% - ${theme.spacing(2)})`,
    height: 0, // min-height doesn't calculate an inheritable height otherwise
  },
  table: {
    paddingBottom: '1px', // Prevent an incorrect height calculation that shows a second scrollbar
    minHeight: 'calc(100% - 1px)',
    height: 0,
  },
}), { name: 'AdminOverview' });

export const AdminOverview = React.memo(() => {
  const history = useHistory();
  const location = useLocation();
  const { path } = useRouteMatch();

  const authClient = React.useMemo(() => GetOAuthProvider(), []);
  const showInvitationsTab = authClient.isInvitationEnabled();
  const InvitationTab: React.ComponentType = React.useMemo(
    // eslint-disable-next-line react-memo/require-memo
    () => (authClient.getInvitationComponent() ?? (() => (<>This tab should not have been reachable.</>))),
    [authClient]);

  const tabList: Array<{
    slug: string, label: string, component: React.ComponentType, enabled: boolean,
  }> = React.useMemo(() => [
    { slug: 'clusters', label: 'Clusters',      component: ClustersTable,   enabled: true },
    { slug: 'keys',     label: 'Keys',          component: KeysOverview,    enabled: true },
    { slug: 'plugins',  label: 'Plugins',       component: PluginsOverview, enabled: true },
    { slug: 'users',    label: 'Users',         component: UsersTable,      enabled: true },
    { slug: 'org',      label: 'Org Settings',  component: OrgSettings,     enabled: true },
    { slug: 'user',     label: 'User Settings', component: UserSettings,    enabled: true },
    { slug: 'invite',   label: 'Invitations',   component: InvitationTab,   enabled: showInvitationsTab },
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
