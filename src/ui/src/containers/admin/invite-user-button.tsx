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

import { Add } from '@mui/icons-material';
import {
  Button,
  Dialog,
  DialogTitle,
  DialogContent,
  TextField,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useFlags } from 'launchdarkly-react-client-sdk';

import { useSnackbar } from 'app/components';
import { getRedirectPath } from 'app/utils/redirect-utils';

const useStyles = makeStyles((theme: Theme) => createStyles({
  fieldContainer: {
    marginTop: theme.spacing(1),
    marginBottom: theme.spacing(1),
    display: 'flex',
    width: '100%',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  textField: {
    flexGrow: 1,
    marginRight: theme.spacing(1),
  },
  heading: {
    ...theme.typography.body1,
    marginBottom: theme.spacing(1),
  },
  body: {
    ...theme.typography.body2,
    lineHeight: 1.6,
  },
  // Doesn't have the same height as the input otherwise
  copyButton: {
    paddingTop: theme.spacing(1),
    paddingBottom: theme.spacing(1),
  },
  expirationWarning: {
    fontSize: '0.75rem',
    color: theme.palette.error.main,
  },
}), { name: 'InviteUserButton' });

export const InviteUserButton = React.memo<{ className: string }>(({ className }) => {
  // TODO: Pull this from somewhere real.
  const inviteId = 'abc123';

  const invitationLink = React.useMemo(() => getRedirectPath('/signup', { invite: inviteId }), [inviteId]);
  const classes = useStyles();

  const [open, setOpen] = React.useState(false);
  const openModal = React.useCallback(() => setOpen(true), []);
  const closeModal = React.useCallback(() => setOpen(false), []);

  const showSnackbar = useSnackbar();
  const copyLink = React.useCallback(async () => {
    try {
      await navigator.clipboard.writeText(invitationLink);
      showSnackbar({ message: 'Copied!' });
    } catch (e) {
      showSnackbar({ message: 'Error: could not copy link automatically.' });
      // eslint-disable-next-line no-console
      console.error(e);
    }
  }, [invitationLink, showSnackbar]);

  const { invite: invitationsEnabled } = useFlags();
  if (!invitationsEnabled) return <></>;

  return (
    <>
      <Button
        onClick={openModal}
        className={className}
        variant='outlined'
        // eslint-disable-next-line react-memo/require-usememo
        startIcon={<Add />}
      >
        Invite Users
      </Button>
      <Dialog open={open} onClose={closeModal}>
        <DialogTitle>Invite Users to Organization</DialogTitle>
        <DialogContent>
          <div className={classes.heading}>Invite Link</div>
          <div className={classes.body}>
            Share this link to invite others to your organization.
            Anyone with this link will automatically join your organization.
          </div>
          <div className={classes.fieldContainer}>
            <TextField
              className={classes.textField}
              /* eslint-disable react-memo/require-usememo */
              onChange={(e) => e.preventDefault()}
              onFocus={(e) => e.target.select()}
              /* eslint-enable react-memo/require-usememo */
              size='small'
              value={invitationLink}
            />
            <Button
              className={classes.copyButton}
              variant='contained'
              size='medium'
              onClick={copyLink}
            >
              Copy Link
            </Button>
          </div>
          <div className={classes.expirationWarning}>
            This link will expire after 7 days. Invite links can be managed in org settings.
          </div>
        </DialogContent>
      </Dialog>
    </>
  );
});
InviteUserButton.displayName = 'InviteUserButton';
