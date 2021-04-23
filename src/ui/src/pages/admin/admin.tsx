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

import { scrollbarStyles } from '@pixie-labs/components';
import {
  Theme,
  createStyles,
  makeStyles,
} from '@material-ui/core/styles';

import * as React from 'react';
import { Route, Switch, Redirect } from 'react-router-dom';
import { LiveViewButton } from 'containers/admin/utils';
import { AdminOverview } from 'containers/admin/admin-overview';
import { ClusterDetails } from 'containers/admin/cluster-details';
import NavBars from 'containers/App/nav-bars';
import { SidebarContext } from 'context/sidebar-context';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    backgroundColor: theme.palette.background.default,
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  title: {
    flexGrow: 1,
    marginLeft: theme.spacing(2),
  },
  main: {
    marginLeft: theme.spacing(6),
    flex: 1,
    minHeight: 0,
    borderTopStyle: 'solid',
    borderTopColor: theme.palette.background.three,
    borderTopWidth: theme.spacing(0.25),
    padding: theme.spacing(1),
  },
  titleText: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    fontWeight: theme.typography.fontWeightBold,
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
        { children }
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
