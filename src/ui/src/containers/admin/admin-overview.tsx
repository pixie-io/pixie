import * as React from 'react';

import {
  createStyles,
  withStyles,
  Theme,
  WithStyles,
} from '@material-ui/core/styles';
import Button from '@material-ui/core/Button';
import TableContainer from '@material-ui/core/TableContainer';
import Add from '@material-ui/icons/Add';

import { DeploymentKeysTable } from 'containers/admin/deployment-keys';
import { APIKeysTable } from 'containers/admin/api-keys';
import { ClustersTable } from 'containers/admin/clusters-list';
import { StyledTab, StyledTabs } from 'containers/admin/utils';
import { scrollbarStyles } from '@pixie-labs/components';
import { useAPIKeys, useDeploymentKeys } from '@pixie-labs/api-react';

export const AdminOverview = withStyles((theme: Theme) => createStyles({
  createButton: {
    margin: theme.spacing(1),
  },
  tabRoot: {
    height: '100%',
    overflowY: 'auto',
    ...scrollbarStyles(theme),
  },
  tabBar: {
    position: 'sticky',
    top: 0,
    display: 'flex',
    background: theme.palette.background.default, // Match the background of the content area to make it look attached.
    zIndex: 1, // Without this, inputs and icons in the table layer on top and break the illusion.
    paddingBottom: theme.spacing(1), // Aligns the visual size of the tab bar with the margin above it.
  },
  tabContents: {
    margin: theme.spacing(1),
  },
  table: {
    paddingBottom: '1px', // Prevent an incorrect height calculation that shows a second scrollbar
  },
}))(({ classes }: WithStyles) => {
  const [{ createDeploymentKey }] = useDeploymentKeys();
  const [{ createAPIKey }] = useAPIKeys();
  const [tab, setTab] = React.useState('clusters');

  return (
    <div className={classes.tabRoot}>
      <div className={classes.tabBar}>
        <StyledTabs
          value={tab}
          onChange={(event, newTab) => setTab(newTab)}
        >
          <StyledTab value='clusters' label='Clusters' />
          <StyledTab value='deployment-keys' label='Deployment Keys' />
          <StyledTab value='api-keys' label='API Keys' />
        </StyledTabs>
        {tab === 'deployment-keys'
          && (
          <Button
            onClick={() => createDeploymentKey()}
            className={classes.createButton}
            variant='outlined'
            startIcon={<Add />}
            color='primary'
          >
            New key
          </Button>
          )}
        {tab === 'api-keys'
          && (
          <Button
            onClick={() => createAPIKey()}
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
        <TableContainer className={classes.table}>
          {tab === 'clusters' && <ClustersTable />}
          {tab === 'deployment-keys' && <DeploymentKeysTable />}
          {tab === 'api-keys' && <APIKeysTable />}
        </TableContainer>
      </div>
    </div>
  );
});
