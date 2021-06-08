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

import {
  CompletionId,
  CompletionItem,
  CompletionItems,
  CompletionTitle,
} from './completions';

export type ItemsMap = Map<
CompletionId,
{ title: CompletionTitle; index: number; type: string }
>;

// Finds the next completion item in the list, given the id of the completion that is currently active.
export const findNextItem = (
  activeItem: CompletionId,
  itemsMap: ItemsMap,
  completions: CompletionItems,
  direction = 1,
): CompletionId => {
  if (completions.length === 0) {
    return '';
  }

  let index = -1;
  if (activeItem !== '') {
    const item = itemsMap.get(activeItem);
    ({ index } = item);
  }
  const { length } = completions;
  for (let i = 1; i <= length; i++) {
    const nextIndex = (index + i * direction + length) % length;
    const next = completions[nextIndex] as CompletionItem;
    if (next.title && next.id) {
      return next.id;
    }
  }
  return activeItem;
};

// Represents a key/value field in the autocomplete command.
export interface TabStop {
  Index: number;
  Label?: string;
  Value?: string;
  CursorPosition: number;
}

// getDisplayStringFromTabStops parses the tabstops to get the display text
// that is shown to the user in the input box.
export const getDisplayStringFromTabStops = (tabStops: TabStop[]): string => {
  let str = '';
  tabStops.forEach((ts, index) => {
    if (ts.Label !== undefined) {
      str += `${ts.Label}:`;
    }
    if (ts.Value !== undefined) {
      str += ts.Value;
    }

    if (index !== tabStops.length - 1) {
      str += ' ';
    }
  });

  return str;
};

// Tracks information about the tabstop, such as what the formatted input should look like and the boundaries of
// each tabstop in the display string.
export class TabStopParser {
  private tabStops: TabStop[];

  private tabBoundaries: [number, number][];

  private input: Array<{ type: 'key'|'value', value: string }>;

  private initialCursor: number;

  constructor(tabStops: Array<TabStop>) {
    this.parseTabStopInfo(tabStops);
  }

  // parseTabStopInfo parses the tabstops into useful display information.
  private parseTabStopInfo = (tabStops: Array<TabStop>) => {
    let cursorPos = 0;
    let currentPos = 0;
    this.tabStops = tabStops;
    this.input = [];
    this.tabBoundaries = [];
    tabStops.forEach((ts, index) => {
      if (ts.Label !== undefined) {
        this.input.push({ type: 'key', value: `${ts.Label}:` });
        currentPos += ts.Label.length + 1;
      }
      if (ts.CursorPosition !== -1) {
        cursorPos = currentPos + ts.CursorPosition;
      }

      const valueStartIndex = currentPos;
      if (ts.Value !== undefined) {
        this.input.push({ type: 'value', value: ts.Value });
        currentPos += ts.Value.length;
      }

      this.tabBoundaries.push([valueStartIndex, currentPos + 1]);

      if (index !== tabStops.length - 1) {
        this.input.push({ type: 'value', value: ' ' });
        currentPos += 1;
      }
    });

    this.initialCursor = cursorPos;
  };

  public getInitialCursor = (): number => this.initialCursor;

  public getTabBoundaries = (): [number, number][] => this.tabBoundaries;

  public getInput = (): Array<{ type: 'key'|'value', value: string }> => this.input;

  // Find the tabstop that the cursor is currently in.
  public getActiveTab = (cursorPos: number): number => {
    let tabIdx = -1;
    this.tabBoundaries.forEach((boundary, index) => {
      if (tabIdx === -1 && cursorPos < boundary[1]) {
        tabIdx = index;
      }
    });

    return tabIdx;
  };

  // handleCompletionSelection returns what the new display string should look like
  // if the completion was made for the given activeTab. It does not actually
  // mutate the contents of the current tabstops.
  public handleCompletionSelection = (
    cursorPos: number,
    completion: { title: CompletionTitle, type: string, index?: number },
  ): [string, number] => {
    const activeTab = this.getActiveTab(cursorPos);

    const newTStops: TabStop[] = [];
    let newCursorPos = -1;

    this.tabStops.forEach((ts, i) => {
      if (i === activeTab) {
        const newTS = { Index: ts.Index } as TabStop;

        if (ts.Label === undefined) {
          newTS.Label = completion.type;
        } else {
          newTS.Label = ts.Label;
        }

        newTS.Value = completion.title;

        if (activeTab > 0) {
          newCursorPos = this.tabBoundaries[activeTab - 1][1];
        }
        newCursorPos += newTS.Label.length + newTS.Value.length + 1;
        newTStops.push(newTS);
      } else {
        newTStops.push({
          Index: ts.Index,
          Label: ts.Label,
          Value: ts.Value,
          CursorPosition: ts.CursorPosition,
        });
      }
    });

    return [getDisplayStringFromTabStops(newTStops), newCursorPos];
  };

  // handleBackspace returns what the new display string should look like
  // if a backspace was made. It does not actually mutate the contents of the
  // current tabstop.
  public handleBackspace = (
    cursorPos: number,
  ): [string, number, Array<TabStop>, boolean] => {
    const activeTab = this.getActiveTab(cursorPos);

    const newTStops = [];
    let newCursor = 0;
    let tabstopDeleted = false;

    this.tabStops.forEach((ts, i) => {
      if (i === activeTab) {
        const pos = cursorPos - this.tabBoundaries[i][0]; // Get the cursor position within the tabstop.

        if (activeTab !== 0 && pos === 0) {
          // User is trying to delete a label. We should move the cursor to the previous tabstop.
          newTStops[i - 1].CursorPosition = newTStops[i - 1].Value == null ? 0 : newTStops[i - 1].Value.length;
          // Subtract 1, for tabstop with no label, or 2 for colon and space between tabstops.
          newCursor = ts.Label == null ? cursorPos - 1 : cursorPos - ts.Label.length - 2;
          tabstopDeleted = true;
        } else if (activeTab === 0 && pos === 0) {
          newCursor = 0;
          const newTS = {
            Index: ts.Index,
            CursorPosition: 0,
          } as TabStop;
          if (ts.Label == null) {
            // If the user just deleted a label, we should also clear the value.
            newTS.Value = ts.Value;
          }
          newTStops.push(newTS);
        } else {
          // We are deleting text inside the tabstop.
          newCursor = cursorPos - 1;
          newTStops.push({
            Index: ts.Index,
            Label: ts.Label,
            Value: ts.Value.substring(0, pos - 1) + ts.Value.substring(pos),
            CursorPosition: pos - 1 <= 0 ? 0 : pos - 1,
          });
        }
      } else {
        newTStops.push({
          Index: ts.Index,
          Label: ts.Label,
          Value: ts.Value,
          CursorPosition: -1,
        });
      }
    });

    return [
      getDisplayStringFromTabStops(newTStops),
      newCursor,
      newTStops,
      tabstopDeleted,
    ];
  };

  // handleBackspace returns what the new display string should look like
  // if the change was made in the current position.
  public handleChange = (input: string, cursorPos: number): Array<TabStop> => {
    const char = input[cursorPos - 1]; // Get character typed by user.
    const activeTab = this.getActiveTab(cursorPos - 1);
    const newTStops = [];

    this.tabStops.forEach((ts, i) => {
      if (i === activeTab) {
        let value = ts.Value;
        let tsCursor: number;
        const pos = cursorPos - 1 - this.tabBoundaries[i][0];
        if (value != null) {
          value = ts.Value.substring(0, pos) + char + ts.Value.substring(pos);
          tsCursor = pos + 1;
        } else {
          value = char;
          tsCursor = 1;
        }
        newTStops.push({
          Index: ts.Index,
          Label: ts.Label,
          CursorPosition: tsCursor,
          Value: value,
        });
      } else {
        newTStops.push({
          Index: ts.Index,
          Label: ts.Label,
          Value: ts.Value,
          CursorPosition: -1, // All non-active tabstops should not have a cursor position.
        });
      }
    });

    return newTStops;
  };
}
