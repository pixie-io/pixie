import { scrollbarStyles } from 'common/mui-theme';
import ProfileMenu from 'containers/profile-menu/profile-menu';
import history from 'utils/pl-history';

import { useMutation } from '@apollo/react-hooks';
import {
  createStyles, makeStyles, Theme,
} from '@material-ui/core/styles';
import Button from '@material-ui/core/Button';
import TableContainer from '@material-ui/core/TableContainer';
import Add from '@material-ui/icons/Add';

import * as React from 'react';
import {
  Link, Route, Router, Switch,
} from 'react-router-dom';
import { StyledTab, StyledTabs } from 'containers/admin/utils';
import { CREATE_DEPLOYMENT_KEY, DeploymentKeysTable } from 'containers/admin/deployment-keys';
import { ClustersTable } from 'containers/admin/clusters-list';
import { ClusterDetailsPage } from 'containers/admin/cluster-details';
import NavBars from 'containers/App/nav-bars';

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
  link: {
    ...theme.typography.subtitle1,
    margin: theme.spacing(1),
    color: theme.palette.text.primary,
  },
  titleText: {
    ...theme.typography.h6,
    color: theme.palette.foreground.one,
    fontWeight: theme.typography.fontWeightBold,
  },
  breadcrumbText: {
    ...theme.typography.subtitle2,
    color: theme.palette.foreground.one,
    fontWeight: theme.typography.fontWeightLight,
  },
  breadcrumbLink: {
    ...theme.typography.subtitle2,
    color: theme.palette.foreground.one,
  },
  createButton: {
    margin: theme.spacing(1),
  },
  tabContents: {
    margin: theme.spacing(1),
  },
  container: {
    maxHeight: 800,
  },
}));

const AdminOverview = () => {
  const [createDeployKey] = useMutation(CREATE_DEPLOYMENT_KEY);
  const classes = useStyles();
  const [tab, setTab] = React.useState('clusters');

  return (
    <div className={classes.root}>
      <NavBars>
        <div className={classes.title}>
          <div className={classes.titleText}>Admin View</div>
        </div>
        <Button component={Link} to='/live' color='primary'>
          Live View
        </Button>
      </NavBars>
      <div className={classes.main}>
        <div style={{ display: 'flex' }}>
          <StyledTabs
            value={tab}
            onChange={(event, newTab) => setTab(newTab)}
          >
            <StyledTab value='clusters' label='Clusters' />
            <StyledTab value='deployment-keys' label='Deployment Keys' />
          </StyledTabs>
          {tab === 'deployment-keys'
            && (
            <Button
              onClick={() => createDeployKey()}
              className={classes.createButton}
              variant='outlined'
              startIcon={<Add />}
              color='primary'
            >
              New key
            </Button>
            )}
        </div>
        <div className={classes.tabContents}>
          <TableContainer className={classes.container}>
            {tab === 'clusters' && <ClustersTable />}
            {tab === 'deployment-keys' && <DeploymentKeysTable />}
          </TableContainer>
        </div>
      </div>
    </div>
  );
};

export default function AdminView() {
  return (
    <Router history={history}>
      <Switch>
        <Route exact path='/admin' component={AdminOverview} />
        <Route exact path='/admin/clusters/:name' component={ClusterDetailsPage} />
      </Switch>
    </Router>
  );
}
