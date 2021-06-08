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

// eslint-disable-next-line import/no-extraneous-dependencies
import { act, renderHook } from '@testing-library/react-hooks';

import { CompletionItems } from './completions';
import { useAutocomplete } from './use-autocomplete';

const MOCK_COMPLETIONS: CompletionItems = [
  { type: 'header', header: 'header1' },
  {
    type: 'item',
    id: 'item1',
    title: 'item1',
  },
  {
    type: 'item',
    id: 'item2',
    title: 'item2',
  },
  { type: 'header', header: 'header2' },
  {
    type: 'item',
    id: 'item3',
    title: 'item3',
  },
];

describe('use-autocomplete hook', () => {
  it('fetches completions on initial render', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue([]);
    const { waitForNextUpdate } = renderHook(() => useAutocomplete(mockGetCompletions, 'initial input'),
    );
    await act(() => waitForNextUpdate());
    expect(mockGetCompletions).toHaveBeenCalledWith('initial input');
  });

  it('fetches completions on input change', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue([]);
    const { rerender, waitForNextUpdate } = renderHook(
      (input: string) => useAutocomplete(mockGetCompletions, input),
      { initialProps: 'initial input' },
    );
    await act(() => waitForNextUpdate());
    expect(mockGetCompletions).toHaveBeenCalledTimes(1);
    await act(() => {
      rerender('new input');
      return waitForNextUpdate();
    });
    expect(mockGetCompletions).toHaveBeenCalledTimes(2);
  });

  it('does not refect completions if the input did not change', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue([]);
    const { rerender, waitForNextUpdate } = renderHook(
      (input: string) => useAutocomplete(mockGetCompletions, input),
      { initialProps: 'initial input' },
    );
    await act(() => waitForNextUpdate());
    expect(mockGetCompletions).toHaveBeenCalledTimes(1);
    await act(() => {
      rerender('initial input');
      return waitForNextUpdate();
    });
    expect(mockGetCompletions).toHaveBeenCalledTimes(1);
  });

  it('highlights the first completion', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue(MOCK_COMPLETIONS);
    const { waitForNextUpdate, result } = renderHook(
      (input: string) => useAutocomplete(mockGetCompletions, input),
      { initialProps: 'initial input' },
    );
    await act(() => waitForNextUpdate());
    expect(result.current.activeCompletion.id).toBe('item1');
  });

  it('highlights by id', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue(MOCK_COMPLETIONS);
    const { waitForNextUpdate, result } = renderHook(
      (input: string) => useAutocomplete(mockGetCompletions, input),
      { initialProps: 'initial input' },
    );
    await act(() => waitForNextUpdate());
    act(() => result.current.highlightById('item3'));
    expect(result.current.activeCompletion.id).toBe('item3');
  });

  it('wraps around to the last completion when moving up from the first', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue(MOCK_COMPLETIONS);
    const { waitForNextUpdate, result } = renderHook(
      (input: string) => useAutocomplete(mockGetCompletions, input),
      { initialProps: 'initial input' },
    );
    await act(() => waitForNextUpdate());
    expect(result.current.activeCompletion.id).toBe('item1');
    act(() => result.current.highlightPrev());
    expect(result.current.activeCompletion.id).toBe('item3');
  });

  it('wraps around to the first completion when moving to next from the last', async () => {
    const mockGetCompletions = jest.fn().mockResolvedValue(MOCK_COMPLETIONS);
    const { waitForNextUpdate, result } = renderHook(
      (input: string) => useAutocomplete(mockGetCompletions, input),
      { initialProps: 'initial input' },
    );
    await act(() => waitForNextUpdate());
    act(() => result.current.highlightById('item3'));
    expect(result.current.activeCompletion.id).toBe('item3');
    act(() => result.current.highlightNext());
    expect(result.current.activeCompletion.id).toBe('item1');
  });
});
