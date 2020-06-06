import { scrollbarStyles } from 'common/mui-theme';
import ProfileMenu from 'containers/live/profile-menu';
import history from 'utils/pl-history';
import {ClusterDetailsPage} from './cluster-details';
import {ClustersTable} from './clusters-list';
import {CREATE_DEPLOYMENT_KEY, DeploymentKeysTable} from './deployment-keys';
import {StyledTab, StyledTabs} from './utils';

import { useMutation } from '@apollo/react-hooks';
import { createStyles, makeStyles, Theme, withStyles } from '@material-ui/core/styles';
import Button from '@material-ui/core/Button';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import Add from '@material-ui/icons/Add';
import * as React from 'react';
import { Link, Route, Router, Switch } from 'react-router-dom';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    root: {
      height: '100%',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: theme.palette.background.default,
      color: theme.palette.text.primary,
      ...scrollbarStyles(theme),
    },
    topBar: {
      display: 'flex',
      margin: theme.spacing(1),
      alignItems: 'center',
    },
    title: {
      flexGrow: 1,
      marginLeft: theme.spacing(2),
    },
    main: {
      flex: 1,
      minHeight: 0,
      borderTopStyle: 'solid',
      borderTopColor: theme.palette.background.three,
      borderTopWidth: theme.spacing(0.25),
    },
    link: {
      ...theme.typography.subtitle1,
      margin: theme.spacing(1),
    },
    titleText: {
      ...theme.typography.h6,
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
    }
  });
});

const AdminOverview = () => {
  const [createDeployKey] = useMutation(CREATE_DEPLOYMENT_KEY);
  const classes = useStyles();
  const [tab, setTab] = React.useState('clusters');

  return (
    <div className={classes.root}>
      <div className={classes.topBar}>
        <div className={classes.title}>
          <div className={classes.titleText}>Admin View</div>
        </div>
        <Link className={classes.link} to='/live'>Live View</Link>
        <ProfileMenu/>
      </div>
      <div className={classes.main}>
        <div style={{display: 'flex'}}>
          <StyledTabs
            value={tab}
            onChange={(event, newTab) => setTab(newTab)}
          >
            <StyledTab value='clusters' label='Clusters' />
            <StyledTab value='deployment-keys' label='Deployment Keys' />
          </StyledTabs>
          {tab === 'deployment-keys' &&
            <Button onClick={() => createDeployKey()}
             className={classes.createButton} variant='outlined'
             startIcon={<Add/>} color='primary'>New key</Button>}
        </div>
        <div style={{margin:'10px'}}>
          {tab === 'clusters' && <ClustersTable/>}
          {tab === 'deployment-keys' && <DeploymentKeysTable/>}
        </div>
      </div>
    </div>
  );
}

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
