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

import { APIKeysTable } from './api-keys';
import { DeploymentKeysTable } from './deployment-keys';
import { StyledTab, StyledTabs } from './utils';

const useStyles = makeStyles((theme: Theme) => createStyles({
  barContainer: {
    position: 'fixed', // Would use 'sticky', but the scroll container is higher up so it doesn't do anything here.
    left: 'auto',
    width: '100%',
    maxWidth: '100%',
    overflowX: 'auto',
    // Illusion: Cover the scroll space surrounding this to hide content that's logically under it
    marginTop: theme.spacing(-2),
    marginLeft: theme.spacing(-2),
    paddingTop: theme.spacing(2),
    paddingLeft: theme.spacing(2),
    backgroundColor: theme.palette.background.default,
    zIndex: 1,
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

  const navTab = React.useCallback((newTab: string) => {
    history.push(`${path}/${newTab}`);
  }, [history, path]);

  return (
    <>
      <div className={classes.barContainer}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <StyledTabs value={tab === 'keys' ? 'api' : tab} onChange={(_, newTab) => navTab(newTab)}>
          <StyledTab value='api' label='API Keys' />
          <StyledTab value='deployment' label='Deployment Keys' />
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
