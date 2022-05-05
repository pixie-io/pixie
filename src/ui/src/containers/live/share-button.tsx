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

import { Send } from '@mui/icons-material';
import {
  IconButton, Tooltip, Dialog, DialogContent, TextField, Button,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { SnackbarContext } from 'app/components';
import { SCRATCH_SCRIPT } from 'app/containers/App/scripts-context';
import { ScriptContext } from 'app/context/script-context';
import pixieAnalytics from 'app/utils/analytics';
import { ShareDialogContent } from 'configurable/share-dialog-content';

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
    marginBottom: theme.spacing(2),
  },
  // Doesn't have the same height as the input otherwise
  copyButton: {
    paddingTop: theme.spacing(1),
    paddingBottom: theme.spacing(1),
  },
}), { name: 'ShareButton' });

const ShareButton = React.memo<{
  classes: Record<'iconButton' | 'iconActive' | 'iconInactive', string>
}>(({ classes: buttonClasses }) => {
  const modalClasses = useStyles();
  const showSnackbar = React.useContext(SnackbarContext);
  const { script } = React.useContext(ScriptContext);

  const [isOpen, setIsOpen] = React.useState(false);
  const openDialog = React.useCallback(() => {
    pixieAnalytics.track('Live View Script Sharing', { action: 'open-dialog', scriptId: script?.id ?? null });
    setIsOpen(true);
  }, [script?.id]);
  const closeDialog = React.useCallback(() => setIsOpen(false), []);

  const tooltip = React.useMemo(() => (
    script?.id === SCRATCH_SCRIPT.id ? 'Sharing is not available for scratchpad scripts' : 'Share this script'
  ), [script]);

  // TODO(nick): This should be built from the current cluster+script+args, instead of trusted as-is.
  const shareLink = global.location.href;

  const copyLink = React.useCallback(async () => {
    try {
      await navigator.clipboard.writeText(shareLink);
      showSnackbar({ message: 'Copied!' });
    } catch (e) {
      showSnackbar({ message: 'Error: could not copy link automatically.' });
      // eslint-disable-next-line no-console
      console.error(e);
    }
    pixieAnalytics.track('Live View Script Sharing', { action: 'copy-url', scriptId: script?.id ?? null });
  }, [script?.id, shareLink, showSnackbar]);

  return (
    <>
      <Tooltip title={tooltip}>
        <span style={{ display: 'inline-block' }}>
          <IconButton
            onClick={openDialog}
            disabled={script?.id === SCRATCH_SCRIPT.id}
            className={buttonClasses.iconButton}
          >
            <Send className={isOpen ? buttonClasses.iconActive : buttonClasses.iconInactive} />
          </IconButton>
        </span>
      </Tooltip>
      <Dialog open={isOpen} onClose={closeDialog}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <DialogContent sx={{ width: ({ spacing }) => spacing(62.5) /* 500px */ }}>
          <div className={modalClasses.heading}>Share Script</div>
          <ShareDialogContent classes={modalClasses} />
          <div className={modalClasses.fieldContainer}>
            <TextField
              className={modalClasses.textField}
              /* eslint-disable react-memo/require-usememo */
              onChange={(e) => e.preventDefault()}
              onFocus={(e) => e.target.select()}
              /* eslint-enable react-memo/require-usememo */
              size='small'
              value={shareLink}
            />
            <Button
              className={modalClasses.copyButton}
              variant='contained'
              size='medium'
              onClick={copyLink}
              disabled={!shareLink}
            >
              Copy Link
            </Button>
          </div>
        </DialogContent>
      </Dialog>
    </>
  );
});
ShareButton.displayName = 'ShareButton';
export default ShareButton;
