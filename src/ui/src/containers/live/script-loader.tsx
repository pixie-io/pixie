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
import { ClusterContext } from 'app/common/cluster-context';
import { ScriptContext } from 'app/context/script-context';
import { useSnackbar } from 'app/components';

/**
 * Automatically runs the selected script whenever it changes, the args change, or the vis spec changes.
 * @constructor
 */
export const ScriptLoader: React.FC = () => {
  const {
    script, args, execute, cancelExecution, manual,
  } = React.useContext(ScriptContext);
  const { selectedClusterName: clusterName } = React.useContext(ClusterContext);

  const showSnackbar = useSnackbar();

  // Sorting keys to ensure stability between identical objects when the route might change their ordering.
  // Sorting the keys in this way loses nested properties, so we're only doing it for args (vis is already stable).
  const serializedArgs = JSON.stringify(args, Object.keys(args ?? {}).sort());
  const serializedVis = JSON.stringify(script?.vis);

  React.useEffect(() => {
    // Wait for everything to be set first.
    if (script == null || args == null) return;
    cancelExecution?.();

    if (
      script
      && serializedArgs
    ) {
      try {
        execute();
      } catch (e) {
        showSnackbar({ message: `Could not execute script: ${e.message}` });
      }
    }

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [script, serializedArgs, serializedVis, clusterName, manual]);

  return null;
};
