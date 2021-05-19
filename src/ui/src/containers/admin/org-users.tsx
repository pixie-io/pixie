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

import {
  makeStyles,
  Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import Button from '@material-ui/core/Button';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import * as React from 'react';
import { useOrgUsers, useUserInfo } from '@pixie-labs/api-react';
import {
  AdminTooltip, StyledTableCell, StyledTableHeaderCell, StyledRightTableCell,
} from './utils';

interface UserDisplay {
  id: string;
  name: string;
  email: string;
  isApproved: boolean;
}

interface UserRowProps {
  user: UserDisplay;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  buttonContainer: {
    display: 'flex',
    justifyContent: 'flex-end',
  },
  approveButton: {
    width: theme.spacing(12),
  },
  error: {
    padding: theme.spacing(1),
  },
}));

export const UserRow: React.FC<UserRowProps> = ({ user }) => {
  const classes = useStyles();

  const [{ updateUserInfo }] = useUserInfo();

  return (
    <TableRow key={user.email}>
      <StyledTableCell>{user.name}</StyledTableCell>
      <StyledTableCell>{user.email}</StyledTableCell>
      <StyledRightTableCell>
        <div className={classes.buttonContainer}>
          <AdminTooltip title='Approved users are users who can login and use Pixie.'>
            <div>
              <Button
                onClick={() => {
                  updateUserInfo(user.id, true);
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
        </div>
      </StyledRightTableCell>
    </TableRow>
  );
};

export const UsersTable: React.FC = () => {
  const [users, loading, error] = useOrgUsers();
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
};
