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

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { scrollbarStyles } from 'app/components';
import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';
import { buildClass } from 'app/utils/build-class';
import { makeCancellable } from 'app/utils/cancellable-promise';

import {
  CompletionId,
  CompletionItem,
  CompletionItems,
  CompletionTitle,
  Completions,
} from './completions';
import { Input } from './input';
import { Key } from './key';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    backgroundColor: theme.palette.background.one,
    cursor: 'text',
    display: 'flex',
    flexDirection: 'column',
    ...scrollbarStyles(theme),
  },
  input: {
    backgroundColor: theme.palette.background.three,
  },
  completions: {
    flex: 1,
    minHeight: 0,
  },
  limitedResultsTip: {
    ...theme.typography.body2,
    width: '100%',
    textAlign: 'right',
    opacity: 0.75,
    fontStyle: 'italic',
    padding: theme.spacing(0.75),
  },
}), { name: 'AutoComplete' });

interface AutoCompleteProps {
  onSelection: (id: CompletionId) => void;
  getCompletions: (input: string) => Promise<{ items: CompletionItems, hasMoreItems: boolean }>;
  placeholder?: string;
  prefix?: React.ReactNode;
  className?: string;
  hint?: React.ReactNode;
}

type ItemsMap = Map<CompletionId, { title: CompletionTitle; index: number }>;

// Scrolls through the list, ignoring autoSelectPriority as the user is interacting directly.
function findNextItem(
  activeItem: CompletionId,
  itemsMap: ItemsMap,
  completions: CompletionItems,
  direction = 1,
): CompletionId {
  if (!activeItem || completions.length === 0) {
    return '';
  }
  const { index } = itemsMap.get(activeItem) ?? { index: 0 };
  const { length } = completions;
  for (let i = 1; i < length; i++) {
    const nextIndex = (index + i * direction + length) % length;
    const next = completions[nextIndex] as CompletionItem;
    if (next.title && next.id) {
      return next.id;
    }
  }
  return activeItem;
}

// Honors autoSelectPriority
function autoSelectItem(completions: CompletionItems): CompletionItem | null {
  const items: CompletionItem[] = completions.filter((c) => c.type === 'item') as CompletionItem[];
  const highestPriority = items.reduce((hi, cmpl) => Math.max(hi, cmpl.autoSelectPriority ?? 0), -Infinity);
  const selectable = items.filter((c) => (c.autoSelectPriority ?? 0) === highestPriority);
  return selectable[0] ?? null;
}

export const Autocomplete = React.memo<AutoCompleteProps>(({
  onSelection,
  getCompletions,
  placeholder,
  prefix,
  className,
  hint,
}) => {
  const classes = useStyles();
  const {
    allowTyping,
    requireCompletion,
    inputRef,
    hidden,
  } = React.useContext(AutocompleteContext);
  const [inputValue, setInputValue] = React.useState('');
  const [completions, setCompletions] = React.useState([]);
  const [hasMoreCompletions, setHasMoreCompletions] = React.useState(false);
  const [activeItem, setActiveItem] = React.useState<CompletionId>('');
  const itemsMap = React.useMemo(() => {
    const map: ItemsMap = new Map();
    completions.forEach((item, index) => {
      if (!item.header) {
        map.set(item.id, { title: item.title, index });
      }
    });
    return map;
  }, [completions]);

  React.useEffect(() => {
    if (hidden) return () => {};
    // Using silent mode because, when Autocomplete is hidden, the listener vanishes.
    // This can result in an uncaught promise rejection when the Autocomplete closes.
    // As the handler is destroyed here, we don't care if the promise never settles.
    const promise = makeCancellable(getCompletions(inputValue));
    promise.then(({ items, hasMoreItems }) => {
      setCompletions(items);
      setHasMoreCompletions(hasMoreItems);
      const selection = autoSelectItem(items);
      if (selection?.title && selection?.id) setActiveItem(selection.id);
    });
    return () => promise.cancel();
  }, [inputValue, getCompletions, hidden]);

  const handleSelection = React.useCallback(
    (id) => {
      // Don't count the empty string (the default completion).
      const item = id ? itemsMap.get(id) : null;
      if (!item && requireCompletion) {
        return;
      }
      if (item) {
        onSelection(id);
        setInputValue(item.title);
      } else {
        onSelection(inputValue);
      }
    },
    [inputValue, requireCompletion, itemsMap, onSelection],
  );

  const handleKey = React.useCallback((key: Key) => {
    switch (key) {
      case 'UP':
        setActiveItem(findNextItem(activeItem, itemsMap, completions, -1));
        break;
      case 'DOWN':
        setActiveItem(findNextItem(activeItem, itemsMap, completions));
        break;
      case 'ENTER':
        handleSelection(activeItem || inputValue.trim());
        break;
      default:
        break;
    }
  }, [activeItem, inputValue, completions, handleSelection, itemsMap]);

  return (
    <div className={buildClass(classes.root, className)}>
      {allowTyping !== false && (
        <Input
          className={classes.input}
          onChange={setInputValue}
          onKey={handleKey}
          value={inputValue}
          placeholder={placeholder}
          prefix={prefix}
          suggestion={itemsMap.get(activeItem)?.title || ''}
          customRef={inputRef}
        />
      )}
      <Completions
        className={classes.completions}
        items={completions}
        onActiveChange={setActiveItem}
        onSelection={handleSelection}
        activeItem={activeItem}
      />
      {hasMoreCompletions && (
        <div className={classes.limitedResultsTip}>
          Showing top matches
        </div>
      )}
      {hint}
    </div>
  );
});
Autocomplete.displayName = 'Autocomplete';
