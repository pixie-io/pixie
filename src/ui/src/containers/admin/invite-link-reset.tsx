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
  Button,
  Dialog,
  DialogTitle,
  DialogContent,
  Typography,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useFlags } from 'launchdarkly-react-client-sdk';

import { useSnackbar } from 'app/components';
import { CancellablePromise, makeCancellable, silentlyCatchCancellation } from 'app/utils/cancellable-promise';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    margin: theme.spacing(2),
  },
  divider: {
    opacity: 0.5,
    marginBottom: theme.spacing(3),
  },
  body: {
    ...theme.typography.body2,
    lineHeight: 1.6,
  },
  actions: {
    display: 'flex',
    justifyContent: 'flex-end',
    alignItems: 'center',
    gap: theme.spacing(1),
    margin: theme.spacing(1),
  },
}), { name: 'InviteLinkReset' });

export const InviteLinkReset = React.memo(() => {
  const classes = useStyles();

  const [open, setOpen] = React.useState(false);
  const [pendingReset, setPendingReset] = React.useState<CancellablePromise<void>|null>(null);
  const openModal = React.useCallback(() => setOpen(true), []);
  const closeModal = React.useCallback(() => {
    setOpen(false);
    pendingReset?.cancel();
    setPendingReset(null);
  }, [pendingReset]);

  const showSnackbar = useSnackbar();
  const resetLinks = React.useCallback(async () => {
    // TODO: This is where the API call goes.
    setPendingReset(makeCancellable(new Promise((resolve) => setTimeout(resolve, 1500))));
  }, []);

  React.useEffect(() => {
    pendingReset
      ?.then(() => {
        showSnackbar({ message: 'Links reset!' });
      })
      .catch(silentlyCatchCancellation)
      .catch((e) => {
        showSnackbar({ message: 'Failed to reset invite links. Please try again later.' });
        // eslint-disable-next-line no-console
        console.error(e);
      })
      .finally(closeModal);
  }, [closeModal, pendingReset, showSnackbar]);

  const { invite: invitationsEnabled } = useFlags();
  if (!invitationsEnabled) return <></>;

  return (
    <div className={classes.root}>
      <hr className={classes.divider} />
      <Typography variant='body1'>Invite Links</Typography>
      <p className={classes.body}>
        Invitations expire 7 days after the are created.<br/>
        You can invalidate all of them now, if needed.
      </p>
      <Button onClick={openModal} variant='contained' color='error'>
        Reset Invite Links
      </Button>
      <Dialog open={open} onClose={closeModal}>
        <DialogTitle>Reset Invite Links?</DialogTitle>
        <DialogContent className={classes.body}>
          This will invalidate all pending invite links. You will need to create a new link to invite more users.
        </DialogContent>
        <div className={classes.actions}>
          <Button onClick={closeModal}>Cancel</Button>
          <Button
            onClick={resetLinks}
            disabled={!!pendingReset}
            color='error'
          >
            Reset Invite Links
          </Button>
        </div>
      </Dialog>
    </div>
  );
});
InviteLinkReset.displayName = 'InviteLinkReset';
