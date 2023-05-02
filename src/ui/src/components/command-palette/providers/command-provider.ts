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

/**
 * A {@link CommandProvider} may offer a button to show to the right of the CommandPalette's input.
 * Individual suggestions from a provider may offer more specific buttons when they're highlighted.
 */
export interface CommandCta {
  label: React.ReactNode;
  /** If `disabled` is set, this doesn't get called when the button is clicked. */
  action: () => void;
  tooltip?: React.ReactNode;
  disabled?: boolean;
}

/** An individual suggestion from a CommandProvider based on the current input and selection. */
export interface CommandCompletion {
  /** If provided, this completion will be grouped with others that share this heading. */
  heading?: string;
  key: string;
  label: React.ReactNode;
  description: React.ReactNode;
  /**
   * Called when the user selects this completion (by clicking it or using the arrow keys and Enter).
   * If this returns a [string, number] pair, the input is changed to the string and the caret moves to the number.
   * Otherwise, there are no extra side effects.
   */
  onSelect: () => [newInput: string, newSelection: number] | void;
  cta?: CommandCta;
}

export interface CommandProviderState {
  input: string;
  selection: [start: number, end: number];
  providerName: string;
  loading: boolean;
  completions: CommandCompletion[];
  hasAdditionalMatches: boolean;
  cta?: CommandCta;
}

export type CommandProviderDispatchAction = (
  // Invoke: tell the provider to start looking for results and to dispatch updates to itself as they come in
  { type: 'invoke', input: string; selection: [start: number, end: number ] }
  // Cancel: tells the provider to stop any running searches and consider itself loaded (if not already)
  | { type: 'cancel' }
);

/**
 * A React hook that returns a [state, dispatch] from useReducer(), to fetch input completions to the command palette.
 * Note: these are consumed in CommandPaletteContextProvider; can't use anything from that context (dependency cycle).
 */
export type CommandProvider = () => [
  state: CommandProviderState,
  dispatch: React.Dispatch<CommandProviderDispatchAction>,
];
