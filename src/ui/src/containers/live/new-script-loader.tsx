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
import { ScriptContext } from 'context/new-script-context';

/**
 * Automatically runs the selected script whenever it changes, the args change, or the vis spec changes.
 * @constructor
 */
export const ScriptLoader: React.FC = () => {
  const {
    script, args, execute, cancelExecution,
  } = React.useContext(ScriptContext);

  // Sorting keys to ensure stability between identical objects when the route might change their ordering.
  // Sorting the keys in this way loses nested properties, so we're only doing it for args (vis is already stable).
  const serializedArgs = JSON.stringify(args, Object.keys(args ?? {}).sort());
  const serializedVis = JSON.stringify(script?.vis);

  React.useEffect(() => {
    // Wait for everything to be set first.
    if (script == null || args == null || script?.vis == null) return;

    /*
     * TODO(nick,PC-917): Run the script only if hasMutation is false. Copy most of the logic from old ScriptLoader.
     *  Re-run if the PxL, scriptID, cluster, vis, or args have changed. ScriptContext.execute handles ResultsContext.
     *  Note: cluster is part of args right now; there's another task in ScriptContext to pull it up a level.
     */

    cancelExecution?.();
    if (script && serializedArgs) execute();

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [script, serializedArgs, serializedVis]);

  return null;
};
