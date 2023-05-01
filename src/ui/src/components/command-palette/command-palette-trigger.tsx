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

import { Search as SearchIcon } from '@mui/icons-material';
import { Dialog, DialogContent } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { LiveShortcutsContext } from 'app/containers/live/shortcuts';

import { CommandPaletteContext } from './command-palette-context';
import { CommandTextField } from './command-text-field';

const useTriggerStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    ...theme.typography.body1,
    color: theme.palette.text.primary,
    backgroundColor: theme.palette.foreground.grey3,
    padding: theme.spacing(.5),
    margin: '0 auto',
    cursor: 'pointer',
    borderRadius: theme.shape.borderRadius,
    border: theme.palette.border.unFocused,
    '&:hover': {
      backgroundColor: theme.palette.foreground.grey2,
      border: theme.palette.border.focused,
    },
    width: theme.breakpoints.values.sm / 2,
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'stretch',
    alignItems: 'center',
  },
  leftIcon: {
    marginLeft: theme.spacing(.5),
    marginRight: theme.spacing(.5),
    fontSize: '1rem',
  },
  info: {
    flex: '1 1 auto',
    display: 'inline-block',
    ...theme.typography.caption,
    opacity: 0.8,
    fontStyle: 'italic',
    textAlign: 'left',
    paddingLeft: theme.spacing(0.5),
  },
  hotKey: {
    display: 'inline-block',
    padding: `0 ${theme.spacing(0.5)}`,
    fontSize: '0.75rem',
    ...theme.typography.body1,
    ...theme.typography.monospace,
    borderRadius: theme.shape.borderRadius,
    border: theme.palette.border.unFocused,
    opacity: 0.8,
    textTransform: 'lowercase',
  },
}), { name: 'CommandPaletteTrigger' });

const useModalStyles = makeStyles((theme: Theme) => createStyles({
  contentRoot: {
    position: 'relative',
    width: `min(${theme.spacing(100)}, 90vw)`,
    height: `min(${theme.spacing(75)}, 90vh)`,
    padding: 0,
    border: theme.palette.border.focused,
    borderRadius: theme.shape.borderRadius,
  },
  previewFeatureWarning: {
    position: 'absolute',
    bottom: theme.spacing(1),
    right: theme.spacing(1),
    color: theme.palette.text.primary,
    fontStyle: 'italic',
    zIndex: 1,
  },
}), { name: 'CommandPaletteModal' });

const TriggerWrapper = React.memo(() => {
  const { 'toggle-command-palette': { displaySequence } } = React.useContext(LiveShortcutsContext);
  const { setOpen } = React.useContext(CommandPaletteContext);
  const classes = useTriggerStyles();

  const onClick: React.MouseEventHandler = React.useCallback((ev) => {
    ev.preventDefault();
    ev.stopPropagation();
    setOpen(true);
  }, [setOpen]);

  return (
    <button
      className={classes.root}
      type='button'
      onClick={onClick}
      aria-label={`Open command palette (shortcut: ${[displaySequence].flat().join('+')})`}
    >
      <SearchIcon className={classes.leftIcon} />
      <span className={classes.info}>Run a command...</span>
      <kbd className={classes.hotKey}>{[displaySequence].flat().join('+')}</kbd>
    </button>
  );
});
TriggerWrapper.displayName = 'TriggerWrapper';

export const CommandPaletteTrigger = React.memo<{ text: string }>(({ text }) => {
  const modalClasses = useModalStyles();
  const { open, setOpen } = React.useContext(CommandPaletteContext);
  const onClose = React.useCallback(() => setOpen(false), [setOpen]);

  return (
    <>
      <TriggerWrapper />
      <Dialog open={open} onClose={onClose} maxWidth={false}>
        <div className={modalClasses.previewFeatureWarning}>
          <span>Preview feature.&nbsp;</span>
          <a
            target='_blank'
            rel='noreferrer'
            // eslint-disable-next-line max-len
            href='https://github.com/pixie-io/pixie/issues/new?assignees=&labels=area/ui&template=bug_report.md&title=Command%20Palette:%20'
          >
              Feedback welcome.
          </a>
        </div>
        <DialogContent className={modalClasses.contentRoot}>
          <CommandTextField text={text} />
        </DialogContent>
      </Dialog>
    </>
  );
});
CommandPaletteTrigger.displayName = 'CommandPaletteTrigger';
