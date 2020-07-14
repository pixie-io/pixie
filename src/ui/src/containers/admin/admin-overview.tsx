import * as React from 'react';

import { useMutation } from '@apollo/react-hooks';
import { Theme, withStyles } from '@material-ui/core/styles';
import Button from '@material-ui/core/Button';
import TableContainer from '@material-ui/core/TableContainer';
import Add from '@material-ui/icons/Add';

import { CREATE_DEPLOYMENT_KEY, DeploymentKeysTable } from 'containers/admin/deployment-keys';
import { ClustersTable } from 'containers/admin/clusters-list';
import { StyledTab, StyledTabs } from 'containers/admin/utils';

export const AdminOverview = withStyles((theme: Theme) => ({
  createButton: {
    margin: theme.spacing(1),
  },
  tabContents: {
    margin: theme.spacing(1),
  },
  container: {
    maxHeight: 800,
  },
}))(({ classes }: any) => {
  const [createDeployKey] = useMutation(CREATE_DEPLOYMENT_KEY);
  const [tab, setTab] = React.useState('clusters');

  return (
    <>
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
    </>
  );
});
