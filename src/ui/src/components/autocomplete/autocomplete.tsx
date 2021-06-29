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

import { buildClass } from 'app/utils/build-class';
import * as React from 'react';

import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

import { scrollbarStyles } from 'app/components';
import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';

import { makeCancellable, silentlyCatchCancellation } from 'app/utils/cancellable-promise';
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
    backgroundColor: theme.palette.background.three,
    cursor: 'text',
    display: 'flex',
    flexDirection: 'column',
    ...scrollbarStyles(theme),
  },
  input: {
    backgroundColor: theme.palette.background.two,
  },
  completions: {
    flex: 1,
    minHeight: 0,
  },
}));

interface AutoCompleteProps {
  onSelection: (id: CompletionId) => void;
  getCompletions: (input: string) => Promise<CompletionItems>;
  placeholder?: string;
  prefix?: React.ReactNode;
  className?: string;
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
  const { index } = itemsMap.get(activeItem);
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
function autoSelectItem(completions: CompletionItems): CompletionItem|null {
  const items: CompletionItem[] = completions.filter((c) => c.type === 'item') as CompletionItem[];
  const highestPriority = items.reduce((hi, cmpl) => Math.max(hi, cmpl.autoSelectPriority ?? 0), -Infinity);
  const selectable = items.filter((c) => (c.autoSelectPriority ?? 0) === highestPriority);
  return selectable[0] ?? null;
}

export const Autocomplete: React.FC<AutoCompleteProps> = ({
  onSelection,
  getCompletions,
  placeholder,
  prefix,
  className,
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
    const promise = makeCancellable(getCompletions(inputValue));
    promise.then((cmpls) => {
      setCompletions(cmpls);
      const selection = autoSelectItem(cmpls);
      if (selection?.title && selection?.id) setActiveItem(selection.id);
    }).catch(silentlyCatchCancellation);
    return () => promise.cancel();
  }, [inputValue, getCompletions, hidden]);

  const handleSelection = React.useCallback(
    (id) => {
      const item = itemsMap.get(id);
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

  const handleKey = (key: Key) => {
    switch (key) {
      case 'UP':
        setActiveItem(findNextItem(activeItem, itemsMap, completions, -1));
        break;
      case 'DOWN':
        setActiveItem(findNextItem(activeItem, itemsMap, completions));
        break;
      case 'ENTER':
        handleSelection(activeItem);
        break;
      default:
        break;
    }
  };

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
    </div>
  );
};
