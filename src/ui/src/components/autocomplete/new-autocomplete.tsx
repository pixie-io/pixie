import clsx from 'clsx';
import { scrollbarStyles } from 'common/mui-theme';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import Completions, { CompletionId, CompletionItem } from './completions';
import { AutocompleteInput } from './new-autocomplete-input';
import {
  TabStop, findNextItem, ItemsMap, TabStopParser,
} from './utils';
import { Key } from './key';

// The amount of time elasped since a user has last typed after which we can make a API call.
const TYPING_WAIT_INTERVAL_MS = 500;

const useStyles = makeStyles((theme: Theme) => (
  createStyles({
    root: {
      backgroundColor: theme.palette.background.three,
      cursor: 'text',
      display: 'flex',
      flexDirection: 'column',
      // TODO(malthus): remove this once the scrollbar theme is set at global level.
      ...scrollbarStyles(theme),
    },
    input: {
      backgroundColor: theme.palette.background.two,
    },
    completions: {
      flex: 1,

      minHeight: 0,
    },
  })
));

// Each tabstop is associated with a list of suggestions. These are the suggestions that
// should be shown when the cursor position is on a specific tabstop.
export interface TabSuggestion {
  index: number;
  // Whether the command becomes valid after a suggestion for this tabstop is chosen. Currently this is unused,
  // but we may use this for optimizations in the future.
  executableAfterSelect: boolean;
  suggestions: CompletionItem[];
}

type AutocompleteAction = 'EDIT' | 'SELECT';

interface NewAutoCompleteProps {
  onSubmit: () => void; // This is called when the user presses enter, and no suggestions are highlighted.
  onChange: (input: string, cursor: number, action: AutocompleteAction, updatedTabStops: TabStop[]) => void;
  completions: Array<TabSuggestion>;
  tabStops: Array<TabStop>;
  prefix?: React.ReactNode;
  className?: string;
  placeholder?: string;
}

export const NewAutocomplete: React.FC<NewAutoCompleteProps> = ({
  onSubmit,
  onChange,
  tabStops,
  completions,
  prefix,
  className,
  placeholder = '',
}) => {
  const classes = useStyles();

  const [cursorPos, setCursorPos] = React.useState(0);
  const [activeCompletions, setActiveCompletions] = React.useState([]);
  const [activeItem, setActiveItem] = React.useState<CompletionId>('');

  // State responsible for tracking whether the user is actively typing. This is used for debouncing.
  const [typing, setTyping] = React.useState(false);
  const [timer, setTimer] = React.useState(null);

  // Parse tabstops to get boundary and input info.
  const tsInfo = React.useMemo(() => (new TabStopParser(tabStops)), [tabStops]);

  React.useEffect(() => {
    setCursorPos(tsInfo.getInitialCursor());
  }, [tsInfo]);

  const itemsMap = React.useMemo(() => {
    const map: ItemsMap = new Map();
    activeCompletions.forEach((item, index) => {
      if (!item.header) {
        map.set(item.id, { title: item.title, index, type: item.itemType });
      }
    });
    return map;
  }, [activeCompletions]);

  // Show different suggestions when cursor position changes.
  React.useEffect(() => {
    if (completions.length === 0) {
      return;
    }

    const tabIdx = tsInfo.getActiveTab(cursorPos);
    if (completions[tabIdx] === undefined) {
      setActiveCompletions([]);
    } else {
      setActiveCompletions(completions[tabIdx].suggestions);
    }
    setActiveItem('');
  }, [cursorPos, completions, tsInfo]);

  const handleSelection = React.useCallback((id) => {
    if (!itemsMap.has(id)) {
      return;
    }
    const item = itemsMap.get(id);
    const [newStr, newCursorPos] = tsInfo.handleCompletionSelection(cursorPos, item);
    onChange(newStr, newCursorPos, 'SELECT', null);
  }, [itemsMap, cursorPos, tsInfo, onChange]);

  const handleBackspace = React.useCallback((pos) => {
    const [newStr, newCursorPos] = tsInfo.handleBackspace(pos);
    return onChange(newStr, newCursorPos, 'EDIT', null);
  }, [tsInfo, onChange]);

  const handleLeftKey = React.useCallback((pos) => {
    const activeTab = tsInfo.getActiveTab(pos);
    const tabBoundaries = tsInfo.getTabBoundaries();
    if (pos - 1 >= tabBoundaries[activeTab][0]) {
      // Cursor is still within the current tabstop.
      setCursorPos(pos - 1);
    } else if (activeTab !== 0) {
      // Cursor should move to the previous tabstop.
      setCursorPos(tabBoundaries[activeTab - 1][1] - 1);
    }
  }, [tsInfo]);

  const handleRightKey = React.useCallback((pos) => {
    const activeTab = tsInfo.getActiveTab(pos);
    const tabBoundaries = tsInfo.getTabBoundaries();

    if (pos + 1 < tabBoundaries[activeTab][1]) {
      // Cursor is still within the current tabstop.
      setCursorPos(pos + 1);
    } else if (activeTab !== tabStops.length - 1) {
      // Cursor should move to the next tabstop.
      setCursorPos(tabBoundaries[activeTab + 1][0]);
    }
  }, [tsInfo, tabStops.length]);

  const handleKey = (key: Key) => {
    switch (key) {
      case 'UP':
        setActiveItem(findNextItem(activeItem, itemsMap, activeCompletions, -1));
        break;
      case 'DOWN':
        setActiveItem(findNextItem(activeItem, itemsMap, activeCompletions));
        break;
      case 'LEFT':
        handleLeftKey(cursorPos);
        break;
      case 'RIGHT':
        handleRightKey(cursorPos);
        break;
      case 'ENTER':
        // If active item is selected, then handle selection. Otherwise, make a request to submit.
        if (activeItem === '') {
          onSubmit();
        } else {
          handleSelection(activeItem);
        }
        break;
      case 'BACKSPACE':
        handleBackspace(cursorPos);
        break;
      default:
    }
  };

  const onChangeHandler = React.useCallback((input: string, pos: number) => {
    setTyping(true);
    clearTimeout(timer);
    const newTimer = setTimeout(() => {
      setTyping(false);
      // This is only triggered if the user has stopped typing for a while.
      onChange(input, pos, 'EDIT', null);
    }, TYPING_WAIT_INTERVAL_MS);
    setTimer(newTimer);

    if (typing) {
      // If the user is actively typing, we should update the tabstops ourselves instead of making an API call to do so.
      onChange(input, pos, 'EDIT', tsInfo.handleChange(input, pos));
      return;
    }

    onChange(input, pos, 'EDIT', null);
  }, [onChange, tsInfo, typing, timer]);

  return (
    <div className={clsx(classes.root, className)}>
      <AutocompleteInput
        className={classes.input}
        cursorPos={cursorPos}
        setCursor={setCursorPos}
        onChange={onChangeHandler}
        onKey={handleKey}
        value={tsInfo.getInput()}
        prefix={prefix}
        placeholder={placeholder}
      />
      <Completions
        className={classes.completions}
        items={activeCompletions}
        onActiveChange={setActiveItem}
        onSelection={handleSelection}
        activeItem={activeItem}
      />
    </div>
  );
};
