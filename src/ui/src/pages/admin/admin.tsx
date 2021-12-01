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
import { createStyles, makeStyles } from '@mui/styles';
import { Route, Switch, Redirect } from 'react-router-dom';

import { scrollbarStyles, Footer } from 'app/components';
import { AdminOverview } from 'app/containers/admin/admin-overview';
import { ClusterDetails } from 'app/containers/admin/cluster-details';
import { LiveViewButton } from 'app/containers/admin/utils';
import NavBars from 'app/containers/App/nav-bars';
import { SidebarContext } from 'app/context/sidebar-context';
import { Copyright } from 'configurable/copyright';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  title: {
    flexGrow: 1,
    marginLeft: theme.spacing(2),
    height: '100%',
  },
  main: {
    marginLeft: theme.spacing(8),
    flex: 1,
    minHeight: 0,
    padding: theme.spacing(1),
    display: 'flex',
    flexFlow: 'column nowrap',
    overflow: 'auto',
  },
  mainBlock: {
    flex: '1 0 auto',
  },
  mainFooter: {
    flex: '0 0 auto',
  },
  titleText: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    fontWeight: theme.typography.fontWeightBold,
    display: 'flex',
    alignItems: 'center',
    height: '100%',
  },
}), { name: 'AdminPage' });

export const AdminPage: React.FC = React.memo(({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <SidebarContext.Provider value={{ showLiveOptions: false, showAdmin: true }}>
        <NavBars>
          <div className={classes.title}>
            <div className={classes.titleText}>Admin</div>
          </div>
          <LiveViewButton />
        </NavBars>
      </SidebarContext.Provider>
      <div className={classes.main}>
        <div className={classes.mainBlock}>
          { children }
        </div>
        <div className={classes.mainFooter}>
          <Footer copyright={Copyright} />
        </div>
      </div>
    </div>
  );
});
AdminPage.displayName = 'AdminPage';

// eslint-disable-next-line react-memo/require-memo
const AdminOverviewPage = () => (
  <AdminPage>
    <AdminOverview />
  </AdminPage>
);
AdminOverviewPage.displayName = 'AdminOverviewPage';

// eslint-disable-next-line react-memo/require-memo
const ClusterDetailsPage = () => (
  <AdminPage>
    <ClusterDetails />
  </AdminPage>
);
ClusterDetailsPage.displayName = 'ClusterDetailsPage';

// eslint-disable-next-line react-memo/require-memo
const AdminView: React.FC = () => (
  <Switch>
    <Route exact path='/admin/clusters/:name' component={ClusterDetailsPage} />
    <Redirect exact from='/admin' to='/admin/clusters' />
    <Route path='/admin' component={AdminOverviewPage} />
  </Switch>
);
AdminView.displayName = 'AdminView';

export default AdminView;
