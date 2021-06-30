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

import { gql, useQuery } from '@apollo/client';
import * as React from 'react';
import {
  Theme, makeStyles,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';
import { ClusterContext } from 'app/common/cluster-context';

import { StatusCell, Select } from 'app/components';
import { GQLClusterInfo, GQLClusterStatus } from 'app/types/schema';
import { clusterStatusGroup } from 'app/containers/admin/utils';

const useStyles = makeStyles(({ spacing, palette }: Theme) => createStyles({
  label: {
    marginRight: spacing(0.5),
    justifyContent: 'center',
    display: 'flex',
    alignItems: 'center',
    color: palette.text.secondary,
    fontWeight: 800,
  },
  container: {
    display: 'flex',
    justifyContent: 'center',
    flexDirection: 'row',
  },
}));

const ClusterSelector: React.FC = () => {
  const classes = useStyles();

  const { data, loading, error } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'id' | 'clusterUID' | 'clusterName' | 'prettyClusterName' | 'status'>[]
  }>(
    gql`
      query listClustersForSelector {
        clusters {
          id
          clusterUID
          clusterName
          prettyClusterName
          status
        }
      }
    `,
    { pollInterval: 15000 },
  );

  const clusters = data?.clusters;
  const { selectedClusterPrettyName, setClusterByName } = React.useContext(ClusterContext);

  const getListItems = React.useCallback(async (input: string) => (
    clusters
      ?.filter((c) => c.status !== GQLClusterStatus.CS_DISCONNECTED && c.clusterName.includes(input))
      .map((c) => ({
        title: c.prettyClusterName,
        value: c.clusterName,
        icon: <StatusCell statusGroup={clusterStatusGroup(c.status)} />,
      }))
      .sort((clusterA, clusterB) => clusterA.title.localeCompare(clusterB.title))
  ), [clusters]);

  if (loading || !clusters || error) return (<></>);

  return (
    <div className={classes.container}>
      <div className={classes.label}>Cluster:</div>
      <Select
        value={selectedClusterPrettyName}
        getListItems={getListItems}
        onSelect={setClusterByName}
        requireCompletion
      />
    </div>
  );
};

export default ClusterSelector;
