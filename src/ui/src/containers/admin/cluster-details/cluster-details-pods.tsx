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

import {
  Help as HelpIcon,
  KeyboardArrowRight as RightIcon,

} from '@mui/icons-material';
import { TableHead, TableRow, Table, TableBody } from '@mui/material';
import { styled, Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { StatusCell, buildClass } from 'app/components';
import {
  StyledTableCell,
  StyledTableHeaderCell,
  AdminTooltip,
  StyledLeftTableCell,
  StyledRightTableCell,
} from 'app/containers/admin/utils';
import { GQLPodStatus } from 'app/types/schema';

import {
  GroupedPodStatus,
  combineReasonAndMessage,
  formatPodStatus,
} from './cluster-details-utils';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    position: 'relative',
    height: '100%',
    display: 'flex',
    flexFlow: 'row nowrap',
    alignItems: 'stretch',
  },
  leftContent: {
    flex: '1 1 60%',
    overflow: 'auto',
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'flex-start',
    alignItems: 'stretch',
    paddingTop: theme.spacing(1),
  },
  rightContent: {
    flex: '1 1 40%',
    overflow: 'auto',
  },
  muted: {
    opacity: 0.8,
  },
  topPadding: {
    paddingTop: theme.spacing(2),
    paddingBottom: theme.spacing(2),
  },
  titleContainer: {
    display: 'flex',
  },
  helpIcon: {
    display: 'flex',
    paddingLeft: theme.spacing(1),
  },
  podTypeHeader: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    height: '100%',
    margin: `0 ${theme.spacing(2)}`,
    padding: `${theme.spacing(1)} 0`,
    flex: '0 1',
    borderBottom: theme.palette.border.unFocused,

    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',

    '& > span:first-child': {
      display: 'flex',
      justifyContent: 'flex-start',
      alignItems: 'center',
    },
  },
  podTypeSecondHeader: {
    marginTop: theme.spacing(8),
  },
  podRow: {
    cursor: 'pointer',
    '&:hover': {
      backgroundColor: theme.palette.background.four,
    },
  },
  selectedPodRow: {
    backgroundColor: theme.palette.background.five,
    '&:hover': {
      backgroundColor: theme.palette.background.six,
    },
  },
  statusSummaries: {
    flex: '0 0 auto',
    display: 'flex',
    gap: theme.spacing(3),
    '& > span': {
      display: 'flex',
      alignItems: 'center',
      gap: theme.spacing(0.75),
    },
  },
  noResults: {
    lineHeight: 3,
    color: theme.palette.text.disabled,
    fontStyle: 'italic',
    paddingLeft: theme.spacing(3),
  },
  sidebar: {
    border: theme.palette.border.unFocused,
    borderRadius: theme.shape.borderRadius,
    backgroundColor: theme.palette.background.default,
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'flex-start',
    height: '100%',

    '& > h1': {
      ...theme.typography.h2,
      margin: theme.spacing(2),
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'flex-start',
    },

    '& td': {
      backgroundColor: 'transparent',
    },
  },
  sidebarHidden: {
    opacity: 0,
  },
  emptySidebarMessage: {
    width: '100%',
    margin: 'auto 0',
    textAlign: 'center',
    color: theme.palette.text.disabled,
    fontStyle: 'italic',
  },
}), { name: 'ClusterDetailsPods' });

// eslint-disable-next-line react-memo/require-memo
const StyledSmallTableCell = styled(StyledTableCell, { name: 'SmallTableCell' })(({ theme }) => ({
  fontWeight: theme.typography.fontWeightLight,
  backgroundColor: theme.palette.foreground.grey2,
  borderWidth: 0,
}));

// eslint-disable-next-line react-memo/require-memo
const StyledTableHeadRow = styled(TableRow, { name: 'StyledTableRow' })(({ theme }) => ({
  '& > th': {
    fontWeight: 'normal',
    textTransform: 'uppercase',
    color: theme.palette.foreground.grey4,
  },
}));

const PodHeader = React.memo(() => (
  <TableHead>
    <StyledTableHeadRow>
      <StyledTableHeaderCell />{/* Status icon */}
      <StyledTableHeaderCell>Name</StyledTableHeaderCell>
      <StyledTableHeaderCell>Status Message</StyledTableHeaderCell>
      {/* eslint-disable-next-line react-memo/require-usememo */}
      <StyledTableHeaderCell sx={{ textAlign: 'right', minWidth: (t) => t.spacing(18) }}>
        Restart Count
      </StyledTableHeaderCell>
      <StyledTableHeaderCell />{/* Expanded icon when active */}
    </StyledTableHeadRow>
  </TableHead>
));
PodHeader.displayName = 'PodHeader';

const PodRow = React.memo<{ podStatus: GroupedPodStatus, isSelected: boolean, toggleSelected: () => void }>(({
  podStatus,
  isSelected,
  toggleSelected,
}) => {
  const classes = useStyles();

  const { name, status, statusGroup } = podStatus;

  return (
    // Fragment shorthand syntax does not support key, which is needed to prevent
    // the console error where a key is not present in a list element.
    // eslint-disable-next-line react/jsx-fragments
    <React.Fragment key={name}>
      <TableRow
        key={name}
        className={buildClass(classes.podRow, isSelected && classes.selectedPodRow)}
        onClick={toggleSelected}
      >
        <AdminTooltip title={status}>
          {/* eslint-disable-next-line react-memo/require-usememo */}
          <StyledLeftTableCell sx={{ width: (t) => t.spacing(3) }}>
            <StatusCell statusGroup={statusGroup} />
          </StyledLeftTableCell>
        </AdminTooltip>
        <StyledTableCell>{name}</StyledTableCell>
        <StyledTableCell>
          {combineReasonAndMessage(podStatus.reason, podStatus.message) || <span className={classes.muted}>None</span>}
        </StyledTableCell>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <StyledTableCell sx={{ textAlign: 'right' }}>{podStatus.restartCount}</StyledTableCell>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <StyledRightTableCell sx={{ width: (t) => t.spacing(3), p: 0 }}>
          {/* eslint-disable-next-line react-memo/require-usememo */}
          <RightIcon sx={{ mt: 0.5, opacity: isSelected ? 1 : 0.25, transition: 'opacity 0.125s linear' }} />
        </StyledRightTableCell>
      </TableRow>
    </React.Fragment>
  );
});
PodRow.displayName = 'PodRow';

const DetailsSidebar = React.memo<{ pod: GroupedPodStatus | null }>(({ pod }) => {
  const classes = useStyles();

  return !pod ? (
    <section
      className={buildClass(classes.sidebar, classes.sidebarHidden)}
      data-testid='cluster-details-pods-sidebar-hidden'
      aria-label='Click a pod row to read details about it here'
    >
      <span className={classes.emptySidebarMessage}>Select a pod on the left</span>
    </section>
  ) : (
    <section
      className={classes.sidebar}
      data-testid='cluster-details-pods-sidebar'
      aria-label={`Details for pod "${pod.name}". Click another pod's row to read its details here instead.`}
    >
      <h1>
        <span>{pod.name}</span>
      </h1>
      <h2 className={classes.podTypeHeader}>Events</h2>
      {pod.events?.length > 0 ? (
        <Table>
          <TableHead>
            <StyledTableHeadRow><StyledTableHeaderCell>Event</StyledTableHeaderCell></StyledTableHeadRow>
          </TableHead>
          <TableBody>
            {pod.events.map((event, i) => (
              <TableRow key={i}><StyledSmallTableCell>{event.message}</StyledSmallTableCell></TableRow>
            ))}
          </TableBody>
        </Table>
      ) : (
        <div className={classes.noResults}>No events to report.</div>
      )}
      <h2 className={classes.podTypeHeader}>Containers</h2>
      {pod.containers?.length > 0 ? (
        <Table>
          <TableHead>
            <StyledTableHeadRow>
              <StyledTableHeaderCell />
              <StyledTableHeaderCell>Container</StyledTableHeaderCell>
              <StyledTableHeaderCell>Status</StyledTableHeaderCell>
            </StyledTableHeadRow>
          </TableHead>
          <TableBody>
            {pod.containers.map((container, i) => (
              <TableRow key={i}>
                <AdminTooltip title={container.state}>
                  {/* eslint-disable-next-line react-memo/require-usememo */}
                  <StyledSmallTableCell sx={{ width: (t) => t.spacing(3) }}>
                    <StatusCell statusGroup={container.statusGroup} />
                  </StyledSmallTableCell>
                </AdminTooltip>
                <StyledSmallTableCell>
                  {container.name}
                </StyledSmallTableCell>
                <StyledSmallTableCell>
                  {combineReasonAndMessage(container.reason, container.message)}
                </StyledSmallTableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      ) : (
        <div className={classes.noResults}>No containers in this pod.</div>
      )}
    </section>
  );
});
DetailsSidebar.displayName = 'DetailsSidebar';

export const PixiePodsTab = React.memo<{
  controlPlanePods: GQLPodStatus[],
  dataPlanePods: GQLPodStatus[],
}>(({ controlPlanePods, dataPlanePods }) => {
  const classes = useStyles();
  const controlPlaneDisplay = React.useMemo(
    () => (controlPlanePods ?? []).map((podStatus) => formatPodStatus(podStatus)),
    [controlPlanePods]);
  const dataPlaneDisplay = React.useMemo(
    () => (dataPlanePods ?? []).map((podStatus) => formatPodStatus(podStatus)),
    [dataPlanePods]);

  const [selectedPod, setSelectedPod] = React.useState<GroupedPodStatus>(null);

  const numHealthyControlPlanePods = React.useMemo(() => controlPlaneDisplay.filter(
    (stat) => stat.statusGroup === 'healthy' as const,
  ).length, [controlPlaneDisplay]);
  const numUnhealthyControlPlanePods = React.useMemo(() => controlPlaneDisplay.filter(
    (stat) => stat.statusGroup !== 'healthy' as const,
  ).length, [controlPlaneDisplay]);
  const numUnhealthyDataPlanePods = React.useMemo(() => dataPlaneDisplay.filter(
    (stat) => stat.statusGroup !== 'healthy' as const,
  ).length, [dataPlaneDisplay]);

  return (
    <div className={classes.root}>
      <div className={classes.leftContent}>
        <div className={classes.podTypeHeader}>
          <span>Control Plane Pods</span>
          <span className={classes.statusSummaries}>
            <AdminTooltip title='Healthy pods'>
              <span>
                <span>{numHealthyControlPlanePods}</span>
                <StatusCell statusGroup='healthy' />
              </span>
            </AdminTooltip>
            <AdminTooltip title='Unhealthy pods'>
              <span>
                <span>{numUnhealthyControlPlanePods}</span>
                <StatusCell statusGroup='unhealthy' />
              </span>
            </AdminTooltip>
          </span>
        </div>
        {controlPlaneDisplay.length > 0 ? (
          <Table>
            <PodHeader />
            <TableBody>
              {controlPlaneDisplay.map((podStatus) => (
                <PodRow
                  key={podStatus.name}
                  podStatus={podStatus}
                  isSelected={selectedPod?.name === podStatus.name}
                  // eslint-disable-next-line react-memo/require-usememo
                  toggleSelected={() => setSelectedPod((prev) => prev?.name === podStatus.name ? null : podStatus)}
                />
              ))}
            </TableBody>
          </Table>
        ) : (
          <div className={classes.noResults}>Cluster has no Pixie control plane pods.</div>
        )}

        <div className={`${classes.podTypeHeader} ${classes.podTypeSecondHeader}`}>
          <AdminTooltip title={'To see a list of all agents, click the Agents tab.'}>
            <span>Sample of Unhealthy Data Plane Pods <HelpIcon className={classes.helpIcon} /></span>
          </AdminTooltip>
          <span className={classes.statusSummaries}>
            {/* No healthy mark here, as this list is already only unhealthy pods */}
            <AdminTooltip title='Unhealthy pods'>
              <span>
                <span>{numUnhealthyDataPlanePods}</span>
                <StatusCell statusGroup='unhealthy' />
              </span>
            </AdminTooltip>
          </span>
        </div>
        {dataPlaneDisplay.length > 0 ? (
          <Table>
            <PodHeader />
            <TableBody>
              {dataPlaneDisplay.map((podStatus) => (
                <PodRow
                  key={podStatus.name}
                  podStatus={podStatus}
                  isSelected={selectedPod?.name === podStatus.name}
                  // eslint-disable-next-line react-memo/require-usememo
                  toggleSelected={() => setSelectedPod((prev) => prev?.name === podStatus.name ? null : podStatus)}
                />
              ))}
            </TableBody>
          </Table>
        ) : (
          <div className={classes.noResults}>Cluster has no unhealthy Pixie data plane pods.</div>
        )}
      </div>
      <div className={classes.rightContent}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <DetailsSidebar pod={selectedPod} />
      </div>
    </div>
  );
});
PixiePodsTab.displayName = 'PixiePodsTab';
