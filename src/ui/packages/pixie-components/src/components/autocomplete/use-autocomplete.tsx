import * as React from 'react';

import { CompletionItem, CompletionItems } from './completions';

export type GetCompletionsFunc = (input: string) => Promise<CompletionItems>;

interface AutocompleteState {
  loading: boolean;
  completions: CompletionItems;
  input: string;
  activeCompletion: CompletionItem;
  highlightPrev: () => void;
  highlightNext: () => void;
  highlightById: (id: string) => void;
  selectById: (id: string) => CompletionItem;
}

// Hook for managing autocomplete states.
export const useAutocomplete = (
  getCompletions: GetCompletionsFunc,
  input: string,
): AutocompleteState => {
  const [completions, setCompletions] = React.useState<CompletionItems>([]);
  const [activeIndex, setActiveIndex] = React.useState<number>(-1);
  const [loading, setLoading] = React.useState<boolean>(false);
  const completionItems: CompletionItem[] = React.useMemo(
    () => completions.filter((c) => c.type === 'item') as CompletionItem[],
    [completions],
  );
  const completionsMap = React.useMemo(() => {
    const map = new Map<string, number>();
    completionItems.forEach((c, i) => {
      map.set(c.id, i);
    });
    return map;
  }, [completionItems]);

  React.useEffect(() => {
    setLoading(true);
    getCompletions(input)
      .then(setCompletions)
      .finally(() => {
        setLoading(false);
      });
  }, [input, getCompletions]);

  React.useEffect(() => {
    if (completionItems.length > 0) {
      setActiveIndex(0);
    }
  }, [completionItems]);

  const { length } = completionItems;
  const highlightNext = () => {
    setActiveIndex((idx) => (idx + 1) % length);
  };

  const highlightPrev = () => {
    setActiveIndex((idx) => (idx - 1 + length) % length);
  };

  const highlightById = (id: string) => {
    const idx = completionsMap.get(id);
    if (typeof idx === 'number' && completionItems[idx]?.type === 'item') {
      setActiveIndex(idx);
    }
  };

  const selectById = (id: string) => {
    const idx = completionsMap.get(id);
    if (typeof idx === 'number' && completionItems[idx]?.type === 'item') {
      return completionItems[idx];
    }
    return null;
  };

  const activeCompletion = activeIndex >= 0 && activeIndex < length
    ? completionItems[activeIndex]
    : null;

  return {
    loading,
    completions,
    input,
    highlightById,
    highlightNext,
    highlightPrev,
    activeCompletion,
    selectById,
  };
};
