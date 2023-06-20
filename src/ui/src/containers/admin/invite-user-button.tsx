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

import { gql, useMutation, useLazyQuery } from '@apollo/client';
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

import OrgContext from 'app/common/org-context';
import { useSnackbar } from 'app/components';
import { OAUTH_PROVIDER } from 'app/containers/constants';
import { GQLOrgInfo } from 'app/types/schema';
import pixieAnalytics from 'app/utils/analytics';
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

interface InviteUserButtonProps {
  className?: string;
  startOpen?: boolean;
  onClose?: () => void;
}

export const InviteUserButton = React.memo<InviteUserButtonProps>(({
  className,
  onClose: onCloseHandler,
  startOpen = true,
}) => {
  const classes = useStyles();
  const [open, setOpen] = React.useState(false);
  const [getOrg, { data: orgData, loading: orgLoading }] = useLazyQuery<{
    org: Pick<GQLOrgInfo, 'id'>
  }>(
    gql`
      query getOrgInfoOnInviteButton {
        org {
          id
        }
      }
    `,
  );

  const [createInviteToken, { data: inviteTokenData }] = useMutation<{ CreateInviteToken: string }, { id: string }>(
    gql`
      mutation CreateInviteToken($id: ID!) {
        CreateInviteToken(orgID: $id)
      }
    `,
  );

  const { org: { domainName } } = React.useContext(OrgContext);

  const openModal = React.useCallback(() => {
    getOrg();
    setOpen(true);
    pixieAnalytics.track('Open Invite Modal', {});
  }, [getOrg]);

  // If told to open immediately, do so.
  React.useEffect(() => {
    if (startOpen) openModal();
  }, [openModal, startOpen]);

  // We want to get the invite token only after orgData is valid
  // and loaded. This should happen when the modal opens up.
  // Managed accounts don't use this feature; they use a different model.
  React.useEffect(() => {
    if (orgLoading || !orgData || domainName) {
      return;
    }
    createInviteToken({ variables: { id: orgData?.org?.id } });
  }, [createInviteToken, orgData, orgLoading, domainName]);

  const invitationLink = React.useMemo(() => {
    if (domainName) {
      return getRedirectPath('/signup');
    }
    if (!inviteTokenData) {
      return '';
    }
    return getRedirectPath('/invite', { invite_token: inviteTokenData?.CreateInviteToken });
  }, [inviteTokenData, domainName]);

  const closeModal = React.useCallback(() => {
    setOpen(false);
    onCloseHandler?.();
  }, [onCloseHandler]);

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

  if (OAUTH_PROVIDER !== 'auth0' && OAUTH_PROVIDER !== 'oidc') return <></>;

  return (
    <>
      <Button
        onClick={openModal}
        className={className}
        variant='outlined'
        // eslint-disable-next-line react-memo/require-usememo
        startIcon={<Add />}
      >
        Invite {domainName ? 'Members' : 'Users'}
      </Button>
      <Dialog open={open} onClose={closeModal}>
        <DialogTitle>Invite Users to Organization</DialogTitle>
        <DialogContent>
          {domainName ? (
            // Managed domain (such as a Google Workspace)
            <div className={classes.body}>
              Invite users to join your Pixie organization with the link below.<br/>
              Invited teammates must select the “Login with Google” option and use their Google Workspace account.<br/>
            </div>
          ) : (
            // Non-managed domain
            <>
              <div className={classes.heading}>Invite Link</div>
              <div className={classes.body}>
                Share this link to invite others to your organization.
                Anyone with this link will automatically join your organization.
              </div>
            </>
          )}
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
              disabled={!invitationLink}
            >
              Copy Link
            </Button>
          </div>
          {!domainName && (
            <div className={classes.expirationWarning}>
              This link will expire after 7 days. Invite links can be managed in org settings.
            </div>
          )}
        </DialogContent>
      </Dialog>
    </>
  );
});
InviteUserButton.displayName = 'InviteUserButton';
