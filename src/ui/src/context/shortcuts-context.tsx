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

type Action = string;

export type Handlers<T extends Action, E extends UIEvent = KeyboardEvent> = {
  [action in T]: (e?: E) => void;
};

export type KeyMap<T extends Action> = {
  [action in T]: {
    sequence: string;
    displaySequence: string | string[];
    description: string;
  }
};

export type ShortcutsContextProps<T extends Action> = {
  [action in T]: KeyMap<T>[action] & { handler: Handlers<T>[action] };
};
