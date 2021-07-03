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

import { scrollbarStyles, Footer } from 'app/components';
import { Copyright } from 'configurable/copyright';
import {
  Theme,
  makeStyles,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

import * as React from 'react';
import { Route, Switch, Redirect } from 'react-router-dom';
import { LiveViewButton } from 'app/containers/admin/utils';
import { AdminOverview } from 'app/containers/admin/admin-overview';
import { ClusterDetails } from 'app/containers/admin/cluster-details';
import NavBars from 'app/containers/App/nav-bars';
import { SidebarContext } from 'app/context/sidebar-context';

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
    marginLeft: theme.spacing(6),
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
}));

export const AdminPage: React.FC = ({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <SidebarContext.Provider value={{ inLiveView: false }}>
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
};

const AdminOverviewPage = () => (
  <AdminPage>
    <AdminOverview />
  </AdminPage>
);

const ClusterDetailsPage = () => (
  <AdminPage>
    <ClusterDetails />
  </AdminPage>
);

const AdminView: React.FC = () => (
  <Switch>
    <Route exact path='/admin/clusters/:name' component={ClusterDetailsPage} />
    <Redirect exact from='/admin' to='/admin/clusters' />
    <Route path='/admin' component={AdminOverviewPage} />
  </Switch>
);

export default AdminView;
