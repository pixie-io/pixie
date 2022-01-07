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
import { useSnackbar } from 'app/components';
import { ResultsContext } from 'app/context/results-context';
import { ScriptContext } from 'app/context/script-context';
import { stableSerializeArgs } from 'app/utils/args-utils';
import { containsMutation } from 'app/utils/pxl';

/**
 * Automatically runs the selected script whenever it changes, the args change, or the vis spec changes.
 * @constructor
 */
export const ScriptLoader: React.FC = React.memo(() => {
  const {
    script, args, execute, cancelExecution, manual,
  } = React.useContext(ScriptContext);
  const { loading: clusterLoading, selectedClusterName: clusterName } = React.useContext(ClusterContext);
  const resultsContext = React.useContext(ResultsContext);

  const showSnackbar = useSnackbar();

  // Sorting keys to ensure stability between identical objects when the route might change their ordering.
  // Sorting the keys in this way loses nested properties, so we're only doing it for args (vis is already stable).
  const serializedArgs = stableSerializeArgs(args);
  const serializedVis = JSON.stringify(script?.vis);

  React.useEffect(() => {
    // Wait for everything to be set first.
    if (script == null || args == null) return;
    cancelExecution?.();

    const hasMutation = containsMutation(script.code);

    if (!clusterLoading && (!hasMutation || manual)) {
      try {
        execute();
      } catch (e) {
        showSnackbar({ message: `Could not execute script: ${e.message}` });
        // eslint-disable-next-line no-console
        console.error(e.toString());
      }
    } else {
      // This can come up when executing a mutation script, then switching clusters. `manual` will not be set after
      // the first execution, but we still need to clear results when we've changed the parameters.
      resultsContext.clearResults();
      // When switching clusters but not scripts, we want to avoid blanking the widgets or marking them with errors.
      // If we're about to run the script as soon as the cluster switch resolves, mark that early for a smooth switch.
      resultsContext.setLoading(!hasMutation || manual);
      resultsContext.setStreaming(false);
    }

    // `manual` is omitted because it's updated together with script, and disabled again when the script executes.
    // Putting it in the dependency array would immediately cancel a script that's run this way.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [script, serializedArgs, serializedVis, clusterName, clusterLoading]);

  return null;
});
ScriptLoader.displayName = 'ScriptLoader';
