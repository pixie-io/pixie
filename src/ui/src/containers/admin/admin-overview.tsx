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

import { gql, useMutation } from '@apollo/client';
import * as React from 'react';

import {
  makeStyles,
  Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import Button from '@material-ui/core/Button';
import TableContainer from '@material-ui/core/TableContainer';
import Add from '@material-ui/icons/Add';

import { DeploymentKeysTable } from 'app/containers/admin/deployment-keys';
import { APIKeysTable } from 'app/containers/admin/api-keys';
import { UsersTable } from 'app/containers/admin/org-users';
import { ClustersTable } from 'app/containers/admin/clusters-list';
import { OrgSettings } from 'app/containers/admin/org-settings';
import { StyledTab, StyledTabs } from 'app/containers/admin/utils';
import { GetOAuthProvider } from 'app/pages/auth/utils';
import { scrollbarStyles } from 'app/components';
import { GQLAPIKey, GQLDeploymentKey } from 'app/types/schema';

import {
  Route, Switch, useHistory, useLocation, useRouteMatch,
} from 'react-router-dom';

const useStyles = makeStyles((theme: Theme) => createStyles({
  createButton: {
    margin: theme.spacing(1),
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
}));

export const AdminOverview: React.FC = () => {
  const history = useHistory();
  const location = useLocation();
  const { path } = useRouteMatch();

  const [createAPIKey] = useMutation<{ CreateAPIKey: GQLAPIKey }, void>(gql`
    mutation CreateAPIKeyFromAdminPage {
      CreateAPIKey {
        id
        key
        desc
        createdAtMs
      }
    }
  `);
  const [createDeploymentKey] = useMutation<
  { CreateDeploymentKey: GQLDeploymentKey }, void
  >(gql`
    mutation CreateDeploymentKeyFromAdminPage{
      CreateDeploymentKey {
        id
        key
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

  const authClient = React.useMemo(() => GetOAuthProvider(), []);
  const InvitationTab = React.useMemo(
    () => (authClient.getInvitationComponent() ?? (() => (<>This tab should not have been reachable.</>))),
    [authClient]);

  const classes = useStyles();

  return (
    <div className={classes.tabRoot}>
      <div className={classes.tabBar}>
        <StyledTabs
          value={tab}
          onChange={(event, newTab) => navTab(newTab)}
        >
          <StyledTab value='clusters' label='Clusters' />
          <StyledTab value='deployment-keys' label='Deployment Keys' />
          <StyledTab value='api-keys' label='API Keys' />
          <StyledTab value='users' label='Users' />
          <StyledTab value='org' label='Org Settings' />
          { authClient.isInvitationEnabled() && <StyledTab value='invite' label='Invitations' /> }
        </StyledTabs>
        {tab.endsWith('deployment-keys')
        && (
          <Button
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
                  key: '00000000-0000-0000-0000-000000000000',
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
            startIcon={<Add />}
          >
            New key
          </Button>
        )}
        {tab.endsWith('api-keys')
        && (
          <Button
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
                  key: '00000000-0000-0000-0000-000000000000',
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
            startIcon={<Add />}
          >
            New key
          </Button>
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
          </Switch>
        </TableContainer>
      </div>
    </div>
  );
};
