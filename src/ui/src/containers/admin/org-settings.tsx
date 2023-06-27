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

import { gql, useMutation, useQuery } from '@apollo/client';
import {
  Table,
  Button,
  TableBody,
  TableHead,
  TableRow,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import OrgContext from 'app/common/org-context';
import { InviteLinkReset } from 'app/containers/admin/invite-link-reset';
import { GQLOrgInfo } from 'app/types/schema';

import {
  StyledTableCell, StyledTableHeaderCell,
} from './utils';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    width: '100%',
    maxWidth: theme.breakpoints.values.lg,
    margin: '0 auto',
  },
  tableHeadRow: {
    '& > th': {
      fontWeight: 'normal',
      textTransform: 'uppercase',
      color: theme.palette.foreground.grey4,
    },
  },
  error: {
    padding: theme.spacing(1),
  },
  button: {
    width: theme.spacing(12),
  },
}), { name: 'OrgSettings' });

export const OrgSettings = React.memo(() => {
  const classes = useStyles();

  const { org: { domainName } } = React.useContext(OrgContext);

  const { data, loading, error } = useQuery<{ org: Pick<GQLOrgInfo, 'id' | 'name' | 'enableApprovals'> }>(
    gql`
      query getSettingsForCurrentOrg{
        org {
          id
          name
          enableApprovals
        }
      }
    `,
    // To avoid a confusing desync, ignore the cache on first fetch.
    { pollInterval: 60000, fetchPolicy: 'network-only', nextFetchPolicy: 'cache-and-network' },
  );
  const org = data?.org;

  const [updateOrgApprovalSetting] = useMutation<
  { UpdateOrgSettings: Pick<GQLOrgInfo, 'id' | 'name' | 'enableApprovals'> },
  { orgID: string, enableApprovals: boolean }
  >(
    gql`
      mutation UpdateOrgApprovalSetting ($orgID: ID!, $enableApprovals: Boolean!) {
        UpdateOrgSettings(orgID: $orgID, orgSettings: { enableApprovals: $enableApprovals}) {
          id
          name
          enableApprovals
        }
      }
    `,
  );

  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }

  return (
    <div className={classes.root}>
      <Table>
        <TableHead>
          <TableRow className={classes.tableHeadRow}>
            <StyledTableHeaderCell>Setting</StyledTableHeaderCell>
            <StyledTableHeaderCell>Description</StyledTableHeaderCell>
            <StyledTableHeaderCell>Action</StyledTableHeaderCell>
          </TableRow>
        </TableHead>
        <TableBody>
          <TableRow key='approvals'>
            <StyledTableCell>Approvals</StyledTableCell>
            <StyledTableCell>
              {
                org.enableApprovals
                  ? 'Disabling will allow any user who registers in the org to log in and use Pixie without approval.'
                  : 'Enabling will require each user who registers in the org to be approved before they can log in.'
              }
            </StyledTableCell>
            <StyledTableCell>
              <Button
                className={classes.button}
                // eslint-disable-next-line react-memo/require-usememo
                onClick={() => {
                  updateOrgApprovalSetting({
                    optimisticResponse: {
                      UpdateOrgSettings: {
                        id: org.id,
                        name: org.name,
                        enableApprovals: !org.enableApprovals,
                      },
                    },
                    variables: { orgID: org.id, enableApprovals: !org.enableApprovals },
                  }).then();
                }}
                variant='outlined'
                color='primary'
              >
                { org.enableApprovals ? 'Disable' : 'Enable' }
              </Button>
            </StyledTableCell>
          </TableRow>
        </TableBody>
      </Table>
      {!domainName && <InviteLinkReset orgID={org?.id} />}
    </div>
  );
});
OrgSettings.displayName = 'OrgSettings';
