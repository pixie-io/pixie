import * as React from 'react';
import { configure, GlobalHotKeys } from 'react-hotkeys';
import { isMac } from 'utils/detect-os';

import Card from '@material-ui/core/Card';
import Modal from '@material-ui/core/Modal';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import { Handlers, KeyMap, ShortcutsContextProps } from 'context/shortcuts-context';

type LiveHotKeyAction =
  'pixie-command' |
  'show-help' |
  'toggle-editor' |
  'toggle-data-drawer' |
  'execute';

interface LiveViewShortcutsProps {
  handlers: Omit<Handlers<LiveHotKeyAction>, 'show-help'>;
}

export function getKeyMap(): KeyMap<LiveHotKeyAction> {
  const seqPrefix = isMac() ? 'Meta' : 'Control';
  const displayPrefix = isMac() ? 'Cmd' : 'Ctrl';
  const withPrefix = (key: string) => ({
    sequence: `${seqPrefix}+${key}`,
    displaySequence: [displayPrefix, key],
  });
  return {
    'pixie-command': {
      ...withPrefix('k'),
      description: 'Activate Pixie Command',
    },
    'toggle-editor': {
      ...withPrefix('e'),
      description: 'Show/hide script editor',
    },
    'toggle-data-drawer': {
      ...withPrefix('d'),
      description: 'Show/hide data drawer',
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

/**
 * Provides access to globally-defined hotkeys, both their shortcuts and their actual handlers. Use this to:
 * - Show the user inline what they've set as their shortcut for some action that's contextually relevant
 * - Programmatically trigger an action as if the user had activated it themselves
 */
export const LiveShortcutsContext = React.createContext<ShortcutsContextProps<LiveHotKeyAction>>(null);

/**
 * A behavior adjustment for hotkey handlers. Blocks triggers for shortcuts that would change text, focus, or selection.
 * Thus, binding `Shift+/` will emit `?` in text fields and perform its bound function otherwise (for example).
 */
const handlerWrapper = (handler) => (e?: KeyboardEvent) => {
  const active = document.activeElement;
  const editable = active?.tagName === 'INPUT'
      || active?.tagName === 'TEXTAREA'
      || (active as HTMLElement|null)?.isContentEditable;
  // Of note: this means the Tab key, if bound, will do its bound function unless tabbing away from an editable element.
  // Recommendation: don't bind Tab in a global shortcut. That isn't a very nice thing to do.
  if (!editable) {
    e?.preventDefault();
    handler();
    return;
  }

  /*
   * The element IS editable. At this point, the handler has been suppressed. We use a heuristic to check if the combo
   * most likely would affect the current text, selection, or focus of the element. If it would, the handler remains
   * suppressed and the element receives the event normally. Otherwise, we run the handler. This is imperfect: users can
   * change their OS shortcuts. However, these assumptions cover the typical defaults for a US-ASCII keyboard layout.
   */
  if ((e?.ctrlKey || e?.metaKey || e?.altKey)
      && !['ArrowLeft', 'ArrowDown', 'ArrowUp', 'ArrowRight', 'Home', 'End', 'PageDown', 'PageUp'].includes(e?.key)) {
    e?.preventDefault();
    handler();
  }
};

/**
 * Keyboard shortcuts declarations for the live view.
 *
 * The keybindings are declared here, handlers can be registered by child components of the live view.
 */
const LiveViewShortcutsProvider: React.FC<LiveViewShortcutsProps> = (props) => {
  // Run this setup once.
  React.useEffect(() => {
    configure({
      // React hotkeys defaults to ignore events from within ['input', 'select', 'textarea'].
      // We want the Pixie command to work from anywhere.
      ignoreTags: ['select'],
    });
  }, []);

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

  const handlers: Handlers<LiveHotKeyAction> = React.useMemo(() => {
    const wrappedHandlers: Handlers<LiveHotKeyAction> = {
      'show-help': handlerWrapper(toggleOpenHelp),
      ...Object.keys(props.handlers).reduce((result, action) => ({
        ...result,
        [action]: handlerWrapper(props.handlers[action]),
      }), {}) as LiveViewShortcutsProps['handlers'],
    };
    return wrappedHandlers;
  }, [props.handlers, toggleOpenHelp]);

  const context = Object.keys(handlers).reduce((result, action) => ({
    ...result,
    [action]: {
      handler: handlers[action],
      ...keyMap[action],
    },
  }), {}) as ShortcutsContextProps<LiveHotKeyAction>;

  return (
    <>
      <GlobalHotKeys keyMap={actionSequences} handlers={handlers} allowChanges />
      <LiveViewShortcutsHelp keyMap={keyMap} open={openHelp} onClose={toggleOpenHelp} />
      <LiveShortcutsContext.Provider value={context}>
        {props.children}
      </LiveShortcutsContext.Provider>
    </>
  );
};

export default LiveViewShortcutsProvider;

interface LiveViewShortcutsHelpProps {
  open: boolean;
  onClose: () => void;
  keyMap: KeyMap<LiveHotKeyAction>;
}

const useShortcutHelpStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    width: '500px',
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
    border: `solid 2px ${theme.palette.background.three}`,
    borderRadius: '5px',
    backgroundColor: theme.palette.background.two,
    textTransform: 'capitalize',
    height: theme.spacing(4),
    minWidth: theme.spacing(4),
    paddingLeft: theme.spacing(1),
    paddingRight: theme.spacing(1),
    textAlign: 'center',
    ...theme.typography.caption,
    lineHeight: '30px',
  },
  row: {
    display: 'flex',
    flexDirection: 'row',
    borderBottom: `solid 1px ${theme.palette.background.three}`,
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
}));

const LiveViewShortcutsHelp = (props: LiveViewShortcutsHelpProps) => {
  const classes = useShortcutHelpStyles();
  const { open, onClose, keyMap } = props;
  const makeKey = (key) => <div className={classes.key} key={key}>{key}</div>;

  const shortcuts = Object.keys(keyMap).map((action) => {
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
    <Modal open={open} onClose={onClose} BackdropProps={{}}>
      <Card className={classes.root}>
        <div className={classes.title}>
          Available Shortcuts
        </div>
        {shortcuts}
      </Card>
    </Modal>
  );
};
