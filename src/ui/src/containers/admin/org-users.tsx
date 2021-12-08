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
  Button, Dialog, DialogContent, DialogTitle,
  Table,
  TableBody,
  TableHead,
  TableRow,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useFlags } from 'launchdarkly-react-client-sdk';

import { useSnackbar } from 'app/components';
import { GQLUserInfo } from 'app/types/schema';

import {
  AdminTooltip, StyledTableCell, StyledTableHeaderCell, StyledRightTableCell,
} from './utils';

type UserDisplay = Pick<GQLUserInfo, 'id' | 'name' | 'email' | 'isApproved'>;

interface UserRowProps {
  user: UserDisplay;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  buttonContainer: {
    display: 'flex',
    justifyContent: 'flex-end',
    gap: theme.spacing(1),
  },
  approveButton: {
    width: theme.spacing(12),
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
}));

const RemoveUserButton = React.memo<{ user: UserDisplay }>(({ user }) => {
  const classes = useStyles();

  const [open, setOpen] = React.useState(false);
  const openModal = React.useCallback(() => setOpen(true), []);
  const closeModal = React.useCallback(() => setOpen(false), []);

  const [pendingRemoval, setPendingRemoval] = React.useState(false);

  const showSnackbar = useSnackbar();
  const removeUser = React.useCallback(() => {
    // TODO: This is where the callback would go.
    setPendingRemoval(true);
    setTimeout(() => {
      showSnackbar({ message: 'Failed to remove user (because this function is not implemented yet)!' });
      setPendingRemoval(false);
      closeModal();
    }, 1000);
  }, [showSnackbar, closeModal]);

  return (
    <>
      <Button onClick={openModal} variant='contained' color='error'>
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

export const UserRow = React.memo<UserRowProps>(({ user }) => {
  const classes = useStyles();
  const { invite: invitationsEnabled } = useFlags();

  const [updateUserInfo] = useMutation<
  { UpdateUserPermissions: UserDisplay }, { id: string, isApproved: boolean }
  >(
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
                { user.isApproved ? 'Approved' : 'Approve' }
              </Button>
            </div>
          </AdminTooltip>
          {invitationsEnabled && (
            <AdminTooltip title={'Removed users can be added back by sending them new invite links.'}>
              <div>
                <RemoveUserButton user={user} />
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
  const { data, loading, error } = useQuery<{ orgUsers: UserDisplay[] }>(
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

  const users = data?.orgUsers;
  const classes = useStyles();

  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }

  return (
    <>
      <Table>
        <TableHead>
          <TableRow>
            <StyledTableHeaderCell>Name</StyledTableHeaderCell>
            <StyledTableHeaderCell>Email</StyledTableHeaderCell>
            <StyledTableHeaderCell />
          </TableRow>
        </TableHead>
        <TableBody>
          {users.map((user: UserDisplay) => (
            <UserRow key={user.email} user={user} />
          ))}
        </TableBody>
      </Table>
    </>
  );
});
UsersTable.displayName = 'UsersTable';
