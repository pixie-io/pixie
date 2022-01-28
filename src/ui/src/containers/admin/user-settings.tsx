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

import { GQLUserSettings } from 'app/types/schema';

import {
  StyledTableCell, StyledTableHeaderCell,
} from './utils';

const useStyles = makeStyles((theme: Theme) => createStyles({
  error: {
    padding: theme.spacing(1),
  },
  button: {
    width: theme.spacing(12),
  },
}), { name: 'UserSettings' });

export const UserSettings = React.memo(() => {
  const classes = useStyles();

  const { data, loading, error } = useQuery<{ userSettings: GQLUserSettings }>(
    gql`
      query getSettingsForCurrentUser{
        userSettings {
          id
          analyticsOptout
        }
      }
    `,
    // To avoid a confusing desync, ignore the cache on first fetch.
    { pollInterval: 60000, fetchPolicy: 'network-only', nextFetchPolicy: 'cache-and-network' },
  );
  const userSettings = data?.userSettings;

  const [updateUserAnalyticsSetting] = useMutation<
  { UpdateUserSettings: GQLUserSettings }, { analyticsOptout: boolean }
  >(
    gql`
      mutation UpdateUserAnalyticOptOut ($analyticsOptout: Boolean!) {
        UpdateUserSettings(settings: { analyticsOptout: $analyticsOptout}) {
          id
          analyticsOptout
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
    <>
      <Table>
        <TableHead>
          <TableRow>
            <StyledTableHeaderCell>Setting</StyledTableHeaderCell>
            <StyledTableHeaderCell>Description</StyledTableHeaderCell>
            <StyledTableHeaderCell>Action</StyledTableHeaderCell>
          </TableRow>
        </TableHead>
        <TableBody>
          <TableRow key='analytics-optout'>
            <StyledTableCell>UI Analytics Opt-out</StyledTableCell>
            <StyledTableCell>
              {
                userSettings.analyticsOptout
                  ? 'Opting in will enable Pixie to track UI analytics for this user.'
                  : 'Opting out will disable Pixie from tracking UI analytics for this user.'
              }
            </StyledTableCell>
            <StyledTableCell>
              <Button
                className={classes.button}
                // eslint-disable-next-line react-memo/require-usememo
                onClick={() => {
                  updateUserAnalyticsSetting({
                    optimisticResponse: {
                      UpdateUserSettings: {
                        id: userSettings.id,
                        analyticsOptout: !userSettings.analyticsOptout,
                      },
                    },
                    variables: { analyticsOptout: !userSettings.analyticsOptout },
                  }).then();
                }}
                variant='outlined'
                color='primary'
              >
                { userSettings.analyticsOptout ? 'Opt in' : 'Opt out' }
              </Button>
            </StyledTableCell>
          </TableRow>
        </TableBody>
      </Table>
    </>
  );
});
UserSettings.displayName = 'UserSettings';
