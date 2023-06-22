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

import { gql, useQuery } from '@apollo/client';
import { Breadcrumbs as MaterialBreadcrumbs } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useHistory } from 'react-router';
import { Link } from 'react-router-dom';

import { ClusterContext, ClusterContextProps } from 'app/common/cluster-context';
import { Breadcrumbs } from 'app/components';
import {
  getClusterDetailsURL,
  StyledTab,
  StyledTabs,
} from 'app/containers/admin/utils';
import {
  GQLClusterInfo,
  GQLClusterStatus as ClusterStatus,
} from 'app/types/schema';
import { WithChildren } from 'app/utils/react-boilerplate';

import { AgentsTab } from './cluster-details-agents';
import { ClusterSummaryTable } from './cluster-details-details';
import { PixiePodsTab } from './cluster-details-pods';
import { useClusterDetailStyles } from './cluster-details-utils';

export const CLUSTER_NAV_GQL = gql`
  query clusterNavigationData{
    clusters {
      id
      clusterName
      prettyClusterName
      status
    }
  }
`;

export const CLUSTER_BY_NAME_GQL = gql`
  query GetClusterByName($name: String!) {
    clusterByName(name: $name) {
      id
      clusterName
      prettyClusterName
      status
      statusMessage
      clusterVersion
      operatorVersion
      vizierVersion
      lastHeartbeatMs
      numNodes
      numInstrumentedNodes
      controlPlanePodStatuses {
        name
        status
        message
        reason
        restartCount
        containers {
          name
          state
          reason
          message
        }
        events {
          message
        }
      }
      unhealthyDataPlanePodStatuses {
        name
        status
        message
        reason
        restartCount
        containers {
          name
          state
          reason
          message
        }
        events {
          message
        }
      }
    }
  }
`;

const useLinkStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    ...theme.typography.body2,
    display: 'flex',
    alignItems: 'center',
    paddingLeft: theme.spacing(1),
    paddingRight: theme.spacing(1),
    height: theme.spacing(3),
    color: theme.palette.foreground.grey5,
  },
}), { name: 'BreadcrumbLink' });
const StyledBreadcrumbLink: React.FC<WithChildren<{ to: string }>> = React.memo(({ children, to }) => {
  const classes = useLinkStyles();
  return <Link className={classes.root} to={to}>{children}</Link>;
});
StyledBreadcrumbLink.displayName = 'StyledBreadcrumbLink';

const useBreadcrumbsStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
  },
  separator: {
    display: 'flex',
    alignItems: 'center',
    color: theme.palette.foreground.one,
    fontWeight: 1000,
    width: theme.spacing(1),
  },
}), { name: 'ClusterDetailsBreadcrumbs' });

const ClusterDetailsNavigationBreadcrumbs = React.memo<{ selectedClusterName: string }>(({ selectedClusterName }) => {
  const history = useHistory();
  const { data, loading, error } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'prettyClusterName' | 'status'>[],
  }>(CLUSTER_NAV_GQL, {});
  const clusters = data?.clusters;

  const selectedClusterPrettyName = React.useMemo(() => {
    if (loading || error || !clusters) return 'unknown cluster';
    for (const { clusterName, prettyClusterName } of clusters) {
      if (clusterName === selectedClusterName) {
        return prettyClusterName;
      }
    }
    return 'unknown cluster';
  }, [loading, error, clusters, selectedClusterName]);

  const breadcrumbs = React.useMemo(() => (loading || error || !clusters || !selectedClusterPrettyName) ? [] : [
    {
      title: 'cluster',
      value: selectedClusterPrettyName,
      selectable: true,
      omitKey: true,
      // eslint-disable-next-line
      getListItems: async (input) => {
        const items = clusters
          .filter((c) => c.status !== ClusterStatus.CS_DISCONNECTED && c.prettyClusterName.indexOf(input) >= 0)
          .map((c) => ({ value: c.prettyClusterName }));
        return { items, hasMoreItems: false };
      },
      onSelect: (input) => {
        history.push(getClusterDetailsURL(
          clusters.find(({ prettyClusterName }) => prettyClusterName === input)?.clusterName));
      },
    },
  ], [clusters, error, history, loading, selectedClusterPrettyName]);

  return <Breadcrumbs breadcrumbs={breadcrumbs} />;
});
ClusterDetailsNavigationBreadcrumbs.displayName = 'ClusterDetailsNavigationBreadcrumbs';

const ClusterDetailsTabs = React.memo<{ clusterName: string }>(({ clusterName }) => {
  const classes = useClusterDetailStyles();
  const [tab, setTab] = React.useState('details');

  const { data, loading, error } = useQuery<{
    clusterByName: Pick<
    GQLClusterInfo,
    'id' |
    'clusterName' |
    'clusterVersion' |
    'operatorVersion' |
    'vizierVersion' |
    'prettyClusterName' |
    'clusterUID' |
    'status' |
    'statusMessage' |
    'controlPlanePodStatuses' |
    'unhealthyDataPlanePodStatuses' |
    'lastHeartbeatMs' |
    'numNodes' |
    'numInstrumentedNodes'
    >
  }>(
    CLUSTER_BY_NAME_GQL,
    // Ignore cache on first fetch, to avoid blinking stale heartbeats.
    {
      variables: { name: clusterName },
      pollInterval: 60000,
      fetchPolicy: 'network-only',
      nextFetchPolicy: 'cache-first',
    },
  );

  const cluster = data?.clusterByName;

  const clusterContext: ClusterContextProps = React.useMemo(() => (cluster && {
    loading: false,
    selectedClusterID: cluster?.id,
    selectedClusterName: cluster?.clusterName,
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterUID: cluster?.clusterUID,
    selectedClusterStatus: cluster?.status,
    selectedClusterStatusMessage: cluster?.statusMessage,
    setClusterByName: () => {},
  }), [cluster]);

  if (loading) {
    return <div className={classes.errorMessage}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.errorMessage}>{error.toString()}</div>;
  }

  if (!cluster) {
    return (
      <>
        <div className={classes.errorMessage}>
          Cluster
          {' '}
          {clusterName}
          {' '}
          not found.
        </div>
      </>
    );
  }

  return (
    <>
      <StyledTabs
        value={tab}
        // eslint-disable-next-line react-memo/require-usememo
        onChange={(event, newTab) => setTab(newTab)}
        // eslint-disable-next-line react-memo/require-usememo
        classes={{ root: classes.tabHeader }}
      >
        <StyledTab value='details' label='Details' />
        <StyledTab value='agents' label='Agents' />
        <StyledTab value='pixie-pods' label='Pixie Pods' />
      </StyledTabs>
      <div className={classes.tabContents}>
        {
          tab === 'details' && (
            <ClusterSummaryTable cluster={cluster} />
          )
        }
        {
          tab === 'agents' && (
            <ClusterContext.Provider value={clusterContext}>
              <AgentsTab cluster={cluster} />
            </ClusterContext.Provider>
          )
        }
        {
          tab === 'pixie-pods' && (
            <PixiePodsTab
              controlPlanePods={cluster.controlPlanePodStatuses}
              dataPlanePods={cluster.unhealthyDataPlanePodStatuses}
            />
          )
        }
      </div>
    </>
  );
});
ClusterDetailsTabs.displayName = 'ClusterDetailsTabs';

export const ClusterDetails = React.memo<{ name: string, headerAffix?: React.ReactNode }>(({ name, headerAffix }) => {
  const breadcrumbsClasses = useBreadcrumbsStyles();
  const classes = useClusterDetailStyles();

  const clusterName = decodeURIComponent(name);

  return (
    <div className={classes.root}>
      <div className={classes.header}>
        <MaterialBreadcrumbs classes={breadcrumbsClasses}>
          <ClusterDetailsNavigationBreadcrumbs selectedClusterName={clusterName} />
        </MaterialBreadcrumbs>
        {headerAffix || null}
      </div>
      <ClusterDetailsTabs clusterName={clusterName} />
    </div>
  );
});
ClusterDetails.displayName = 'ClusterDetails';
