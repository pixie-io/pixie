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
import { Tooltip } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { ClusterContext } from 'app/common/cluster-context';
import { StatusCell, Select, buildClass } from 'app/components';
import { clusterStatusGroup } from 'app/containers/admin/utils';
import { GQLClusterInfo, GQLClusterStatus } from 'app/types/schema';

const useStyles = makeStyles(({ shape, spacing, typography, palette, zIndex }: Theme) => createStyles({
  borderWrapper: {
    transition: 'all 0.125s linear',
    borderRadius: spacing(2),
    border: `1px ${palette.foreground.grey1} solid`,
    padding: spacing(0.375),
    paddingLeft: spacing(1.5),
    display: 'flex',
    justifyContent: 'center',
    flexDirection: 'row',
  },
  borderWrapperOpen: {
    backgroundColor: palette.background.three,
    position: 'relative',
    // Illusion to merge this element's border with the popover
    borderRadius: 0,
    borderTopLeftRadius: shape.borderRadius,
    borderTopRightRadius: shape.borderRadius,
    '&::after': {
      pointerEvents: 'none',
      color: 'transparent',
      content: '"\u00a0"', // nbsp
      fontSize: '0.01px',
      // TODO: This isn't covering the border of the <Popover />. Same trick worked for breadcrumbs, but not here.
      //  The stacking context for the modal is higher than that of the topbar (this makes sense).
      //  The `transform-style: preserve-3d` trick was not enough to break out of the stacking context.
      //  Likely, Material is doing something wonky to ensure its own behavior is consistent that makes this not work?
      //  It isn't that something else in our code is using z-index or `transform:`, but probably some other way to
      //  generate a stacking context is in there (the topbar itself has one, obviously).
      //  But why would any of that break the `transform-style` workaround?
      //  Might have to put this little line in a <Portal> or something.
      //  Can't really ignore it, gotta make these things consistent or it'll look real bad with multiple on the page.
      //  Layer view in Chrome didn't help, either. That feature is pretty buggy, actually.
      //    Is that feature unmaintained? Firefox removed their version of the feature a few years ago...
      zIndex: zIndex.modal + 1,
      position: 'absolute',
      bottom: '-2px',
      left: 0,
      width: '100%',
      height: 0,
      borderBottom: `2px ${palette.background.three} solid`,
    },
  },
  labelWrapper: {
    ...typography.body2,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'baseline',
    flexDirection: 'row',
  },
  labelPrefix: {
    marginRight: spacing(0.5),
    fontWeight: typography.fontWeightMedium,
  },
  clusterName: {
    ...typography.monospace,
    color: palette.primary.main,
  },
  status: {
    alignSelf: 'center',
    lineHeight: 0,
    marginRight: spacing(0.5),
  },
}), { name: 'ClusterSelector' });

// eslint-disable-next-line react-memo/require-memo
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
    // Other queries frequently update the cluster cache, so don't make excessive network calls.
    { pollInterval: 15000, fetchPolicy: 'cache-first' },
  );

  const clusters = data?.clusters;
  const {
    selectedClusterPrettyName,
    selectedClusterStatus,
    setClusterByName,
    selectedClusterStatusMessage,
  } = React.useContext(ClusterContext);

  const getListItems = React.useCallback(async (input: string) => {
    const items = clusters
      ?.filter((c) => c.status !== GQLClusterStatus.CS_DISCONNECTED && c.clusterName.includes(input))
      .map((c) => ({
        title: c.prettyClusterName,
        value: c.clusterName,
        icon: <StatusCell statusGroup={clusterStatusGroup(c.status)} />,
      }))
      .sort((clusterA, clusterB) => clusterA.title.localeCompare(clusterB.title));
    return { items, hasMoreItems: false };
  }, [clusters]);

  const statusGroup = clusterStatusGroup(selectedClusterStatus);

  const selectedLabel = React.useMemo(() => (
    <div className={classes.labelWrapper}>
      {statusGroup !== 'healthy' && (
        <Tooltip title={`Status: ${selectedClusterStatusMessage}`}>
          <div className={classes.status}>
            <StatusCell statusGroup={statusGroup} />
          </div>
        </Tooltip>
      )}
      <div className={classes.labelPrefix}>cluster:</div>
      <span className={classes.clusterName}>{selectedClusterPrettyName}</span>
    </div>
  ), [classes, selectedClusterPrettyName, selectedClusterStatusMessage, statusGroup]);

  const [selectOpen, setSelectOpen] = React.useState(false);

  if (loading || !clusters || error) return (<></>);

  return (
    <div className={buildClass([classes.borderWrapper, selectOpen && classes.borderWrapperOpen])}>
      <Select
        value={selectedLabel}
        getListItems={getListItems}
        onSelect={setClusterByName}
        requireCompletion
        setOpen={setSelectOpen}
      />
    </div>
  );
};
ClusterSelector.displayName = 'ClusterSelector';

export default ClusterSelector;
