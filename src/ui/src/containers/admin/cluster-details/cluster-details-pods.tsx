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
  KeyboardArrowDown as DownIcon,
  KeyboardArrowUp as UpIcon,
} from '@mui/icons-material';
import { TableHead, TableRow, IconButton, TableCell, Table, TableBody } from '@mui/material';
import { styled } from '@mui/material/styles';

import { StatusCell } from 'app/components';
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
  usePodRowStyles,
  combineReasonAndMessage,
  useClusterDetailStyles,
  formatPodStatus,
} from './cluster-details-utils';

// eslint-disable-next-line react-memo/require-memo
const StyledSmallTableCell = styled(StyledTableCell, { name: 'SmallTableCell' })(({ theme }) => ({
  fontWeight: theme.typography.fontWeightLight,
  backgroundColor: theme.palette.foreground.grey2,
  borderWidth: 0,
}));

const PodHeader = React.memo(() => (
  <TableHead>
    <TableRow>
      <StyledTableHeaderCell />
      <StyledTableHeaderCell>Name</StyledTableHeaderCell>
      <StyledTableHeaderCell>Status</StyledTableHeaderCell>
      <StyledTableHeaderCell>Restart Count</StyledTableHeaderCell>
      <StyledTableHeaderCell />
    </TableRow>
  </TableHead>
));
PodHeader.displayName = 'PodHeader';

const ExpandablePodRow = React.memo<{ podStatus: GroupedPodStatus }>(({ podStatus }) => {
  const {
    name, status, statusGroup, containers, events,
  } = podStatus;
  const [open, setOpen] = React.useState(false);
  const classes = usePodRowStyles();

  return (
    // Fragment shorthand syntax does not support key, which is needed to prevent
    // the console error where a key is not present in a list element.
    // eslint-disable-next-line react/jsx-fragments
    <React.Fragment key={name}>
      <TableRow key={name}>
        <AdminTooltip title={status}>
          {/* eslint-disable-next-line react-memo/require-usememo */}
          <StyledLeftTableCell sx={{ width: (t) => t.spacing(3) }}>
            <StatusCell statusGroup={statusGroup} />
          </StyledLeftTableCell>
        </AdminTooltip>
        <StyledTableCell>{name}</StyledTableCell>
        <StyledTableCell>{combineReasonAndMessage(podStatus.reason, podStatus.message)}</StyledTableCell>
        <StyledTableCell>{podStatus.restartCount}</StyledTableCell>
        <StyledRightTableCell align='right'>
          {/* eslint-disable-next-line react-memo/require-usememo */}
          <IconButton size='small' onClick={() => setOpen(!open)}>
            {open ? <UpIcon /> : <DownIcon />}
          </IconButton>
        </StyledRightTableCell>
      </TableRow>
      {
        open && (
          <TableRow key={`${name}-details`}>
            {/* eslint-disable-next-line react-memo/require-usememo */}
            <TableCell style={{ border: 0 }} colSpan={6}>
              {
                (events && events.length > 0) && (
                  <div>
                    Events:
                    <ul className={classes.eventList}>
                      {events.map((event, i) => (
                        <li key={`${name}-${i}`}>
                          {event.message}
                        </li>
                      ))}
                    </ul>
                  </div>
                )
              }
              <Table size='small' className={classes.smallTable}>
                <TableHead>
                  <TableRow key={name}>
                    <StyledTableHeaderCell />
                    <StyledTableHeaderCell>Container</StyledTableHeaderCell>
                    <StyledTableHeaderCell>Status</StyledTableHeaderCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {containers.map((container) => (
                    // Fragment shorthand syntax does not support key, which is needed to prevent
                    // the console error where a key is not present in a list element.
                    // eslint-disable-next-line react/jsx-fragments
                    <React.Fragment key={container.name}>
                      <TableRow key={`${name}-info`}>
                        <AdminTooltip title={container.state}>
                          <StyledSmallTableCell>
                            <StatusCell statusGroup={container.statusGroup} />
                          </StyledSmallTableCell>
                        </AdminTooltip>
                        <StyledSmallTableCell>{container.name}</StyledSmallTableCell>
                        <StyledSmallTableCell>
                          {combineReasonAndMessage(container.reason, container.message)}
                        </StyledSmallTableCell>
                      </TableRow>
                    </React.Fragment>
                  ))}
                </TableBody>
              </Table>
            </TableCell>
          </TableRow>
        )
      }
    </React.Fragment>
  );
});
ExpandablePodRow.displayName = 'ExpandablePodRow';

export const PixiePodsTab = React.memo<{
  controlPlanePods: GQLPodStatus[],
  dataPlanePods: GQLPodStatus[],
}>(({ controlPlanePods, dataPlanePods }) => {
  const classes = useClusterDetailStyles();
  const controlPlaneDisplay = controlPlanePods.map((podStatus) => formatPodStatus(podStatus));
  const dataPlaneDisplay = dataPlanePods.map((podStatus) => formatPodStatus(podStatus));

  return (
    <>
      <div className={`${classes.podTypeHeader} ${classes.topPadding}`}> Control Plane Pods </div>
      <Table>
        <PodHeader />
        <TableBody>
          {controlPlaneDisplay.map((podStatus) => (
            <ExpandablePodRow key={podStatus.name} podStatus={podStatus} />
          ))}
        </TableBody>
        {/* We place this header as a row so our data plane pods are aligned with the control plane pods.
        We also make the column span 3 so it sizes with the State, Name, and Status column. Without
        this specification, the cell would default to colspan="1" which would cause the state column
        to be extremely wide. If you ever reduce the number of columns in the table you'll want to reduce
        this value.
        */}
        <TableRow>
          <TableCell className={classes.podTypeHeader} colSpan={3}>
            <div className={classes.titleContainer}>
              <div className={classes.titleContainer}>
                Sample of Unhealthy Data Plane Pods
            </div>
              <AdminTooltip title={'Sample of unhealthy Pixie data plane pods. '
                + 'To see a list of all agents, click the Agents tab.'}>
                <HelpIcon className={classes.helpIcon} />
              </AdminTooltip>
            </div>
          </TableCell>
        </TableRow>
        <PodHeader />
        <TableBody>
          {
            dataPlanePods?.length > 0
              ? dataPlaneDisplay.map((podStatus) => (
                <ExpandablePodRow key={podStatus.name} podStatus={podStatus} />
              )) : (
                <TableRow>
                  {/* eslint-disable-next-line react-memo/require-usememo */}
                  <TableCell sx={{ width: (t) => t.spacing(3) }}/>
                  <TableCell colSpan={3}>
                    Cluster has no unhealthy Pixie data plane pods.
                  </TableCell>
                </TableRow>
              )
          }
        </TableBody>
      </Table>
    </>
  );
});
PixiePodsTab.displayName = 'PixiePodsTab';
