import * as React from 'react';
import {configure, GlobalHotKeys} from 'react-hotkeys';
import {isPropertySignature} from 'typescript';
import {isMac} from 'utils/detect-os';

type HotKeyAction =
  'pixie-command' |
  'show-help' |
  'toggle-editor' |
  'toggle-data-drawer';

type KeyMap = {
  [action in HotKeyAction]: string | string[];
};

export type Handlers = Partial<{
  [action in HotKeyAction]: () => void;
}>;

interface LiveViewShortcutsProps {
  handlers: Handlers;
}

/**
 * Keyboard shortcuts declarations for the live view.
 *
 * The keybindings are declared here, handlers can be registered by child components of the live view.
 */
const LiveViewShortcuts = (props: LiveViewShortcutsProps) => {
  // Run this setup once.
  React.useEffect(() => {
    configure({
      // React hotkeys defaults to ignore events from within ['input', 'select', 'textarea'].
      // We want the Pixie command to work from anywhere.
      ignoreTags: ['select'],
    });
  }, []);

  const keyMap: KeyMap = React.useMemo(() => {
    const prefix = isMac() ? 'Meta' : 'Control';
    const withPrefix = (key: string) => `${prefix}+${key}`;

    return {
      'show-help': '?',
      'pixie-command': withPrefix('k'),
      'toggle-editor': withPrefix('e'),
      'toggle-data-drawer': withPrefix('d'),
    };
  }, []);

  const handlers = React.useMemo(() => {
    const handlerWrapper = (handler) => (e) => {
      e.preventDefault();
      handler();
    };
    const wrappedHandlers = {};
    for (const action of Object.keys(props.handlers)) {
      wrappedHandlers[action] = handlerWrapper(props.handlers[action]);
    }
    return wrappedHandlers;
  }, [props.handlers]);
  return <GlobalHotKeys keyMap={keyMap} handlers={handlers} />;
};

export default LiveViewShortcuts;
