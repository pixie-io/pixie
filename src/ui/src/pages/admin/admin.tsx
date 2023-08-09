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

import { alpha } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Route, Switch, Redirect, useLocation } from 'react-router-dom';

import { scrollbarStyles, Footer } from 'app/components';
import { AdminOverview } from 'app/containers/admin/admin-overview';
import { LiveViewButton } from 'app/containers/admin/utils';
import NavBars from 'app/containers/App/nav-bars';
import { SidebarContext } from 'app/context/sidebar-context';
import { WithChildren } from 'app/utils/react-boilerplate';
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
  titleDivider: {
    borderRight: `1px ${alpha(theme.palette.foreground.grey5, 0.25)} solid`,
    height: '25%',
    margin: `0 ${theme.spacing(2)}`,
  },
}), { name: 'AdminPage' });

export const AdminPage: React.FC<WithChildren> = React.memo(({ children }) => {
  const classes = useStyles();

  const { pathname } = useLocation();

  const titleLabel = React.useMemo(() => {
    const tabSlugs = pathname.split('/');
    const tabSlug = tabSlugs[tabSlugs.indexOf('admin') + 1];
    switch (tabSlug) {
      case 'clusters':
        return 'Clusters';
      case 'keys':
        return 'Keys';
      case 'plugins':
        return 'Plugins';
      case 'users':
        return 'Users';
      case 'org':
        return 'Org Settings';
      case 'user':
        return 'User Settings';
      case 'invite':
        return 'Invitations';
      default:
        return '';
    }
  }, [pathname]);

  const titleText = React.useMemo(() => {
    return titleLabel ? (
      <div className={classes.titleText}>
        <span>Admin</span>
        <span className={classes.titleDivider} />
        <span>{titleLabel}</span>
      </div>
    ) : (
      <div className={classes.titleText}>
        <span>Admin</span>
      </div>
    );
  }, [classes.titleText, classes.titleDivider, titleLabel]);

  return (
    <div className={classes.root}>
      <SidebarContext.Provider value={{ showLiveOptions: false, showAdmin: true }}>
        <NavBars>
          <div className={classes.title}>
            {titleText}
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
const AdminView: React.FC = () => (
  <Switch>
    <Redirect exact from='/admin' to='/admin/clusters' />
    <Route path='/admin' component={AdminOverviewPage} />
  </Switch>
);
AdminView.displayName = 'AdminView';

export default AdminView;
