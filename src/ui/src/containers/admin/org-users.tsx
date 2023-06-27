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
  Alert,
  Button,
  Dialog, DialogContent, DialogTitle,
  Table,
  TableBody,
  TableHead,
  TableRow,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import * as QueryString from 'query-string';
import { Link, useHistory } from 'react-router-dom';

import OrgContext from 'app/common/org-context';
import { useSnackbar } from 'app/components';
import { OAUTH_PROVIDER } from 'app/containers/constants';
import { GQLOrgInfo, GQLUserInfo } from 'app/types/schema';

import { InviteUserButton } from './invite-user-button';
import {
  AdminTooltip, StyledTableCell, StyledTableHeaderCell, StyledRightTableCell,
} from './utils';

type UserDisplay = Pick<GQLUserInfo, 'id' | 'name' | 'email' | 'isApproved'>;

interface UserRowProps {
  user: UserDisplay;
  numUsers: number;
  domainName: string;
}

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
  buttonContainer: {
    display: 'flex',
    justifyContent: 'flex-end',
    gap: theme.spacing(1),
  },
  approveButton: {
    width: theme.spacing(12),
  },
  inviteButtonWrapper: {
    width: '100%',
    padding: theme.spacing(2),
    paddingTop: 0,
    display: 'flex',
    justifyContent: 'flex-end',
  },
  error: {
    padding: theme.spacing(1),
  },
  removeDialogActions: {
    display: 'flex',
    justifyContent: 'flex-end',
    alignItems: 'center',
    gap: theme.spacing(1),
    margin: theme.spacing(1),
  },
  managedDomainBanner: {
    margin: `0 ${theme.spacing(2)}`,
    '& p:first-child': { marginTop: 0 },
    '& p:last-child': { marginBottom: 0 },
  },
}), { name: 'OrgUsers' });

const RemoveUserButton = React.memo<UserRowProps>(({ user, numUsers }) => {
  const classes = useStyles();

  const [open, setOpen] = React.useState(false);
  const openModal = React.useCallback(() => setOpen(true), []);
  const closeModal = React.useCallback(() => setOpen(false), []);

  const [pendingRemoval, setPendingRemoval] = React.useState(false);

  const [removeUserMutation] = useMutation<{ RemoveUserFromOrg: boolean }, { userID: string }>(
    gql`
      mutation RemoveUserFromOrg($userID: ID!) {
        RemoveUserFromOrg(userID: $userID)
      }
    `,
  );

  const showSnackbar = useSnackbar();
  const removeUser = React.useCallback(() => {
    setPendingRemoval(true);
    removeUserMutation({
      variables: { userID: user.id },
      update: (cache, { data }) => {
        if (!data.RemoveUserFromOrg) {
          return;
        }
        cache.modify({
          fields: {
            users(existingUsers, { readField }) {
              return existingUsers.filter(
                (u) => (user.id !== readField('id', u)),
              );
            },
          },
        });
      },
      optimisticResponse: { RemoveUserFromOrg: true },
    }).then(() => {
      showSnackbar({ message: `Removed "${user.email}" from org` });
    }).catch((e) => {
      showSnackbar({ message: `Could not remove user: ${e.message}` });
      // eslint-disable-next-line no-console
      console.error(e);
    }).finally(() => {
      setPendingRemoval(false);
      closeModal();
    });
  }, [closeModal, removeUserMutation, showSnackbar, user]);

  return (
    <>
      <Button onClick={openModal} variant='contained' color='error' disabled={numUsers <= 1}>
        Remove
      </Button>
      <Dialog open={open} onClose={closeModal}>
        <DialogTitle>{`Remove ${user.name}?`}</DialogTitle>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <DialogContent sx={{ lineHeight: 1.6 }}>
          <p>
            {/* eslint-disable-next-line max-len */}
            Once {user.name} is removed, they will no longer have access to any resources in the organization,
            including clusters and data.
          </p>
          <p>
            API keys created by this user will also be deleted.
          </p>
        </DialogContent>
        <div className={classes.removeDialogActions}>
          <Button onClick={closeModal}>Cancel</Button>
          <Button
            onClick={removeUser}
            disabled={pendingRemoval}
            color='error'
          >
            Remove
          </Button>
        </div>
      </Dialog>
    </>
  );
});
RemoveUserButton.displayName = 'RemoveUserButton';

export const UserRow = React.memo<UserRowProps>(({ user, numUsers, domainName }) => {
  const classes = useStyles();

  const [updateUserInfo] = useMutation<{ UpdateUserPermissions: UserDisplay }, { id: string, isApproved: boolean }>(
    gql`
      mutation UpdateUserApproval ($id: ID!, $isApproved: Boolean) {
        UpdateUserPermissions(userID: $id, userPermissions: {isApproved: $isApproved}) {
          id
          name
          email
          isApproved
        }
      }
    `,
  );

  const removeTooltip = domainName ?
    'Removed users can join the org again by using Google SSO and signing in with their Google Workspace account.' :
    'Removed users can be added back by sending them new invite links.';

  return (
    <TableRow key={user.email}>
      <StyledTableCell>{user.name}</StyledTableCell>
      <StyledTableCell>{user.email}</StyledTableCell>
      <StyledRightTableCell>
        <div className={classes.buttonContainer}>
          <AdminTooltip title='Approved users are users who can login and use Pixie.'>
            <div>
              <Button
                // eslint-disable-next-line react-memo/require-usememo
                onClick={() => {
                  updateUserInfo({
                    optimisticResponse: {
                      UpdateUserPermissions: {
                        id: user.id,
                        name: user.name,
                        email: user.email,
                        isApproved: true,
                      },
                    },
                    variables: { id: user.id, isApproved: true },
                  }).then();
                }}
                className={classes.approveButton}
                disabled={user.isApproved}
                variant='outlined'
                color='primary'
              >
                {user.isApproved ? 'Approved' : 'Approve'}
              </Button>
            </div>
          </AdminTooltip>
          {OAUTH_PROVIDER !== 'hydra' && (
            <AdminTooltip title={removeTooltip}>
              <div>
                <RemoveUserButton user={user} numUsers={numUsers} domainName={domainName} />
              </div>
            </AdminTooltip>
          )}
        </div>
      </StyledRightTableCell>
    </TableRow>
  );
});
UserRow.displayName = 'UserRow';

export const UsersTable = React.memo(() => {
  const { org: { domainName } } = React.useContext(OrgContext);

  const { data: settings } = useQuery<{ org: Pick<GQLOrgInfo, 'enableApprovals'> }>(
    gql`
      query getSettingsForCurrentOrg{
        org {
          id
          enableApprovals
        }
      }
    `,
    // To avoid a confusing desync, ignore the cache on first fetch.
    { pollInterval: 60000, fetchPolicy: 'network-only', nextFetchPolicy: 'cache-and-network' },
  );
  const enableApprovals = settings?.org.enableApprovals ?? false;

  const { data, error } = useQuery<{ orgUsers: UserDisplay[] }>(
    gql`
      query getUsersForCurrentOrg{
        orgUsers {
          id
          name
          email
          isApproved
        }
      }
    `,
    { pollInterval: 60000, fetchPolicy: 'cache-and-network' },
  );

  const users = data?.orgUsers || [];
  const classes = useStyles();

  const history = useHistory();
  const showInviteDialog = QueryString.parse(location.search).invite === 'true';
  const onCloseInviteDialog = React.useCallback(() => {
    if (showInviteDialog) {
      // So that a page refresh after closing the dialog doesn't immediately open it again
      history.replace({ search: '' });
    }
  }, [history, showInviteDialog]);

  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }

  return (
    <div className={classes.root}>
      <div className={classes.inviteButtonWrapper}>
        <InviteUserButton startOpen={showInviteDialog} onClose={onCloseInviteDialog} />
      </div>
      {domainName && (
        <Alert icon={false} className={classes.managedDomainBanner} color='info' variant='outlined'>
          <p>
            This organization is managed by <strong>{domainName}</strong>.
          </p>
          <p>
            Membership is restricted to users within the <strong>{domainName}</strong> Google Workspace.
            <br/>
            {enableApprovals ? (
              <>
                Users signing up with their managed email must be <strong>manually approved</strong>.
                This can be changed by <Link to='/admin/org'>disabling approvals</Link>.
              </>
            ) : (
              <>
                Users signing up with their managed email will be <strong>automatically approved</strong>.
                This can be changed by <Link to='/admin/org'>enabling approvals</Link>.
              </>
            )}
          </p>
        </Alert>
      )}
      <Table>
        <TableHead>
          <TableRow className={classes.tableHeadRow}>
            <StyledTableHeaderCell>Name</StyledTableHeaderCell>
            <StyledTableHeaderCell>Email</StyledTableHeaderCell>
            <StyledTableHeaderCell />
          </TableRow>
        </TableHead>
        <TableBody>
          {users.map((user: UserDisplay) => (
            <UserRow key={user.email} user={user} numUsers={users.length} domainName={domainName}/>
          ))}
        </TableBody>
      </Table>
    </div>
  );
});
UsersTable.displayName = 'UsersTable';
