import { scrollbarStyles } from 'pixie-components';
import {
  withStyles,
  Theme,
  createStyles,
  WithStyles,
} from '@material-ui/core/styles';

import * as React from 'react';
import { Route, Router, Switch } from 'react-router-dom';
import { LiveViewButton } from 'containers/admin/utils';
import { AdminOverview } from 'containers/admin/admin-overview';
import { ClusterDetails } from 'containers/admin/cluster-details';
import NavBars from 'containers/App/nav-bars';
import history from 'utils/pl-history';
import { PropsWithChildren } from 'react';
import { SidebarContext } from 'context/sidebar-context';

export const AdminPage = withStyles((theme: Theme) => createStyles({
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
}))(({ children, classes }: PropsWithChildren<WithStyles>) => (
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
));

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

export default function AdminView() {
  return (
    <Router history={history}>
      <Switch>
        <Route exact path='/admin' component={AdminOverviewPage} />
        <Route exact path='/admin/clusters/:name' component={ClusterDetailsPage} />
      </Switch>
    </Router>
  );
}
