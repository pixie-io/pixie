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

import { useMutation, gql } from '@apollo/client';
import { Add as AddIcon } from '@mui/icons-material';
import { Button } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { makeStyles, createStyles } from '@mui/styles';
import {
  Redirect,
  Route,
  Switch,
  useHistory,
  useLocation,
  useRouteMatch,
} from 'react-router-dom';

import { GQLAPIKeyMetadata, GQLDeploymentKeyMetadata } from 'app/types/schema';

import { APIKeysTable } from './api-keys';
import { DeploymentKeysTable } from './deployment-keys';
import { StyledTab, StyledTabs } from './utils';

const useStyles = makeStyles((theme: Theme) => createStyles({
  barContainer: {
    position: 'fixed', // Would use 'sticky', but the scroll container is higher up so it doesn't do anything here.
    left: 'auto',
    width: `calc(100% - ${theme.spacing(8)})`, // Account for all the stuff to the left of the container
    maxWidth: '100%',
    overflowX: 'auto',
    // Illusion: Cover the scroll space surrounding this to hide content that's logically under it
    marginTop: theme.spacing(-2),
    marginLeft: theme.spacing(-2),
    paddingTop: theme.spacing(2),
    paddingLeft: theme.spacing(2),
    paddingRight: theme.spacing(2),
    zIndex: 1,
  },
  tabsRoot: {
    backgroundColor: theme.palette.background.default,
    width: '100%',
    maxWidth: theme.breakpoints.values.lg,
    margin: '0 auto',
  },
  tabExtras: {
    position: 'absolute',
    right: 0,
    bottom: 0,
    height: theme.spacing(6),
    display: 'flex',
    flexFlow: 'row nowrap',
    alignItems: 'center',
    marginRight: theme.spacing(2),
  },
  content: {
    marginTop: theme.spacing(6),
  },
}), { name: 'KeysOverview' });

export const KeysOverview = React.memo(() => {
  const classes = useStyles();

  const { path } = useRouteMatch();
  const { pathname } = useLocation();
  const history = useHistory();
  const tab = pathname.split('/').slice(-1)[0];

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

  const navTab = React.useCallback((newTab: string) => {
    history.push(`${path}/${newTab}`);
  }, [history, path]);

  const onAddClick = React.useMemo(() => {
    if (pathname.endsWith('/api')) {
      return () => createAPIKey({
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
      });
    } else if (pathname.endsWith('/deployment')) {
      return () => createDeploymentKey({
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
      });
    } else {
      return null;
    }
  }, [createAPIKey, createDeploymentKey, pathname]);

  return (
    <>
      <div className={classes.barContainer}>
        <StyledTabs
          className={classes.tabsRoot}
          value={tab === 'keys' ? 'api' : tab}
          // eslint-disable-next-line react-memo/require-usememo
          onChange={(_, newTab) => navTab(newTab)}
        >
          <StyledTab value='api' label='API Keys' />
          <StyledTab value='deployment' label='Deployment Keys' />
          {onAddClick != null && (
            <div className={classes.tabExtras}>
              <Button
                onClick={onAddClick}
                variant='outlined'
                // eslint-disable-next-line react-memo/require-usememo
                startIcon={<AddIcon />}
              >
                New key
              </Button>
            </div>
          )}
        </StyledTabs>
      </div>
      <div className={classes.content}>
        <Switch>
          <Redirect exact from={path} to={`${path}/api`} />
          <Route path={`${path}/api`} component={APIKeysTable} />
          <Route path={`${path}/deployment`} component={DeploymentKeysTable} />
        </Switch>
      </div>
    </>
  );
});
KeysOverview.displayName = 'KeysOverview';
