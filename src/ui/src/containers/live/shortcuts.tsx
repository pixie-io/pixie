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

import { Card, Modal } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { GlobalHotKeys } from 'react-hotkeys';

import { Handlers, KeyMap, ShortcutsContextProps } from 'app/context/shortcuts-context';
import { isMac } from 'app/utils/detect-os';
import { WithChildren } from 'app/utils/react-boilerplate';

type LiveHotKeyAction =
  'show-help' |
  'toggle-editor' |
  'toggle-data-drawer' |
  'toggle-command-palette' |
  'execute';

interface LiveViewShortcutsProps {
  handlers: Partial<Handlers<LiveHotKeyAction>>;
}

export function getKeyMap(): KeyMap<LiveHotKeyAction | 'command-palette-cta'> {
  const seqPrefix = isMac() ? 'Meta' : 'Control';
  const displayPrefix = isMac() ? 'Cmd' : 'Ctrl';
  const withPrefix = (key: string) => ({
    sequence: `${seqPrefix}+${key}`,
    displaySequence: [displayPrefix, key],
  });
  return {
    'toggle-editor': {
      ...withPrefix('e'),
      description: 'Show/hide script editor',
    },
    'toggle-data-drawer': {
      ...withPrefix('d'),
      description: 'Show/hide data drawer',
    },
    'toggle-command-palette': {
      ...withPrefix('k'),
      description: 'Show/hide command palette',
    },
    /*
      This one is a bit special.
      Normally, we would not use <GlobalHotKeys> because override behavior is not great there.
      However, we HAVE to use <GlobalHotKeys> instead of <HotKeys>, because the editor (Monaco) creates elements and key
      handlers and focus that lives outside of React. <HotKeys> doesn't pay attention outside of React.

      Since we're stuck with <GlobalHotKeys>, we can still get the desired override behavior by defining two actions
      with the same shortcut and only defining an action for one of them at a time (see pages/live/live.tsx for that).
      This also requires the Command Palette to use a <GlobalHotKeys> for it to all work.

      This is all so that we can have two features working at the same time:
      - Ctrl/Cmd+Enter works while the editor is open and focused
      - If the Command Palette is open, it hijacks what Ctrl/Cmd+Enter does for itself
    */
    'command-palette-cta': {
      ...withPrefix('enter'),
      // No description -> filtered out of the help menu (this one is an implementation detail, after all)
      description: '',
    },
    execute: {
      ...withPrefix('enter'),
      description: 'Execute current Live View script',
    },
    'show-help': {
      sequence: 'shift+?', // For some reason just '?' doesn't work.
      displaySequence: '?',
      description: 'Show all keyboard shortcuts',
    },
  };
}

interface LiveViewShortcutsHelpProps {
  open: boolean;
  onClose: () => void;
  keyMap: KeyMap<LiveHotKeyAction>;
}

const useShortcutHelpStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    width: theme.spacing(62.5), // 500px
    position: 'absolute',
    top: '50%',
    left: '50%',
    transform: 'translate(-50%, -50%)',
  },
  title: {
    ...theme.typography.subtitle2,
    padding: theme.spacing(2),
  },
  key: {
    border: `solid 2px ${theme.palette.background.five}`,
    borderRadius: '5px',
    backgroundColor: theme.palette.background.four,
    textTransform: 'capitalize',
    height: theme.spacing(4),
    minWidth: theme.spacing(4),
    paddingLeft: theme.spacing(1),
    paddingRight: theme.spacing(1),
    textAlign: 'center',
    ...theme.typography.caption,
    lineHeight: theme.spacing(3.75), // 30px; caption.fontSize is 14px
  },
  row: {
    display: 'flex',
    flexDirection: 'row',
    borderBottom: `solid 1px ${theme.palette.background.five}`,
    alignItems: 'center',
  },
  sequence: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
    justifyContent: 'center',
    padding: theme.spacing(1.5),
  },
  description: {
    flex: 3,
  },
}), { name: 'LiveShortcutsHelp' });

const LiveViewShortcutsHelp = React.memo<LiveViewShortcutsHelpProps>(({ open, onClose, keyMap }) => {
  const classes = useShortcutHelpStyles();
  const makeKey = (key) => <div className={classes.key} key={key}>{key}</div>;

  const shortcuts = Object.keys(keyMap).filter(k => keyMap[k].description.length > 0).map((action) => {
    const shortcut = keyMap[action];
    let sequence: React.ReactNode;
    if (Array.isArray(shortcut.displaySequence)) {
      const keys = [];
      shortcut.displaySequence.forEach((key) => {
        keys.push(makeKey(key));
        keys.push('+');
      });
      keys.pop();
      sequence = keys;
    } else {
      sequence = makeKey(shortcut.displaySequence);
    }
    return (
      <div className={classes.row} key={action}>
        <div className={classes.sequence}>
          {sequence}
        </div>
        <div className={classes.description}>{shortcut.description}</div>
      </div>
    );
  });

  return (
    <Modal open={open} onClose={onClose}>
      <Card className={classes.root}>
        <div className={classes.title}>
          Available Shortcuts
        </div>
        {shortcuts}
      </Card>
    </Modal>
  );
});
LiveViewShortcutsHelp.displayName = 'LiveViewShortcutsHelp';

/**
 * Provides access to globally-defined hotkeys, both their shortcuts and their actual handlers. Use this to:
 * - Show the user inline what they've set as their shortcut for some action that's contextually relevant
 * - Programmatically trigger an action as if the user had activated it themselves
 */
export const LiveShortcutsContext = React.createContext<ShortcutsContextProps<LiveHotKeyAction>>(null);
LiveShortcutsContext.displayName = 'LiveShortcutsContext';

/**
 * A behavior adjustment for hotkey handlers. Blocks triggers for shortcuts that would change text, focus, or selection.
 * Thus, binding `Shift+/` will emit `?` in text fields and perform its bound function otherwise (for example).
 */
const handlerWrapper = (handler) => (e?: KeyboardEvent) => {
  const active = document.activeElement;
  const editable = active?.tagName === 'INPUT'
      || active?.tagName === 'TEXTAREA'
      || (active as HTMLElement | null)?.isContentEditable;

  const allowedComboInEditable = (e?.ctrlKey || e?.metaKey || e?.altKey)
      && !['ArrowLeft', 'ArrowDown', 'ArrowUp', 'ArrowRight', 'Home', 'End', 'PageDown', 'PageUp'].includes(e?.key);

  /*
   * The shortcut handler is run UNLESS any of the following are true:
   * - The key combination pressed would alter the value of the focused editable element
   * - It would move focus out of the currently-focused editable element
   * - It would move the cursor or selection within the currently-focused editable element
   * The heuristic for this is imperfect, as users can change their OS hotkeys. This covers many common setups, though.
   */
  if (!editable || allowedComboInEditable) {
    e?.preventDefault?.();
    handler();
  }
};

/**
 * Keyboard shortcuts declarations for the live view.
 *
 * The keybindings are declared here, handlers can be registered by child components of the live view.
 */
const LiveViewShortcutsProvider: React.FC<WithChildren<LiveViewShortcutsProps>> = React.memo(({
  handlers,
  children,
}) => {
  const [openHelp, setOpenHelp] = React.useState(false);
  const toggleOpenHelp = React.useCallback(() => setOpenHelp((cur) => !cur), []);

  const keyMap: KeyMap<LiveHotKeyAction> = React.useMemo(getKeyMap, []);
  const actionSequences = React.useMemo(() => {
    const map = {};
    Object.keys(keyMap).forEach((key) => {
      map[key] = keyMap[key].sequence;
    });
    return map;
  }, [keyMap]);

  const wrappedHandlers: Partial<Handlers<LiveHotKeyAction>> = React.useMemo(() => {
    const wrapped: Partial<Handlers<LiveHotKeyAction>> = {
      'show-help': handlerWrapper(toggleOpenHelp),
      ...Object.keys(handlers).reduce((result, action) => ({
        ...result,
        [action]: handlerWrapper(handlers[action]),
      }), {}) as LiveViewShortcutsProps['handlers'],
    };
    return wrapped;
  }, [handlers, toggleOpenHelp]);

  const context = React.useMemo(() => Object.keys(wrappedHandlers).reduce((result, action) => ({
    ...result,
    [action]: {
      handler: wrappedHandlers[action],
      ...keyMap[action],
    },
  }), {}) as ShortcutsContextProps<LiveHotKeyAction>, [wrappedHandlers, keyMap]);

  // react-hotkeys makes an element, and that element needs to not interfere with the CSS surrounding it
  const wrapStyle = React.useMemo(() => ({ width: '100%', height: '100%' }), []);
  return (
    // eslint-disable-next-line react-memo/require-usememo
    <GlobalHotKeys keyMap={actionSequences} handlers={wrappedHandlers} allowChanges style={wrapStyle}>
      <LiveViewShortcutsHelp keyMap={keyMap} open={openHelp} onClose={toggleOpenHelp} />
      <LiveShortcutsContext.Provider value={context}>
        {children}
      </LiveShortcutsContext.Provider>
    </GlobalHotKeys>
  );
});
LiveViewShortcutsProvider.displayName = 'LiveViewShortcutsProvider';

export default LiveViewShortcutsProvider;
