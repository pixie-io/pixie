import ClusterContext from 'common/cluster-context';
import { CLUSTER_STATUS_DISCONNECTED,
         CLUSTER_STATUS_HEALTHY } from 'common/vizier-grpc-client-context';
import { StatusCell } from 'components/status/status';
import { Spinner } from 'components/spinner/spinner';
// TODO(malthus): Move this to a common location.
import { clusterStatusGroup } from 'containers/admin/utils';

import gql from 'graphql-tag';
import * as React from 'react';

import { useQuery } from '@apollo/react-hooks';
import Button, { ButtonProps } from '@material-ui/core/Button';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import Menu, { MenuProps } from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';
import { withStyles } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import ArrowDropDownIcon from '@material-ui/icons/ArrowDropDown';

const StyledMenu = (props: MenuProps) => (
  <Menu
    elevation={0}
    getContentAnchorEl={null}
    anchorOrigin={{
      vertical: 'bottom',
      horizontal: 'center',
    }}
    transformOrigin={{
      vertical: 'top',
      horizontal: 'center',
    }}
    {...props}
  />
);

const StyledButton = withStyles((theme) => ({
  root: {
    textTransform: 'none',
    color: theme.palette.text.primary,
  },
}))(React.forwardRef(function ButtonWithRef(props: ButtonProps, ref: React.Ref<HTMLButtonElement>) {
  return (
    <Button
      ref={ref}
      variant='outlined'
      color='primary'
      {...props}
    />
  );
}));

const LIST_CLUSTERS = gql`
{
  clusters {
    id
    clusterName
    status
  }
}
`;

export default function ClusterSelector(props: { className: string }) {
  const { selectedCluster, setCluster } = React.useContext(ClusterContext);

  const { loading, data } = useQuery(LIST_CLUSTERS);

  const [anchorEl, setAnchorEl] = React.useState<null | HTMLElement>(null);

  const handleClick = (event: React.MouseEvent<HTMLElement>) => {
    setAnchorEl(event.currentTarget);
  };

  const handleClose = () => {
    setAnchorEl(null);
  };

  if (loading) {
    return <StyledButton className={props.className} disabled={true}><Spinner /></StyledButton>;
  }
  const clusterName = data.clusters.find((c) => c.id === selectedCluster)?.clusterName || 'unknown cluster';
  return (
    <div className={props.className}>
      <Tooltip title='Select Cluster'>
        <StyledButton onClick={handleClick} endIcon={<ArrowDropDownIcon />}>
          {clusterName}
        </StyledButton>
      </Tooltip>
      <StyledMenu
        anchorEl={anchorEl}
        keepMounted
        open={Boolean(anchorEl)}
        onClose={handleClose}
      >
        {
          data.clusters
            .filter((c) => c.status !== CLUSTER_STATUS_DISCONNECTED)
            .map((c) => {
              const statusGroup = clusterStatusGroup(c.status);
              return (
                <MenuItem
                  disabled={c.status !== CLUSTER_STATUS_HEALTHY}
                  key={c.id}
                  dense={true}
                  onClick={() => {
                    setCluster(c.id);
                    handleClose();
                  }}>
                  <ListItemIcon>
                    <StatusCell statusGroup={statusGroup} />
                  </ListItemIcon>
                  <ListItemText primary={c.clusterName} />
                </MenuItem>
              );
            })
        }
      </StyledMenu>
    </div>
  );
}
