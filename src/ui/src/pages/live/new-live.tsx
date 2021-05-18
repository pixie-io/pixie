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
import { VizierContextRouter } from 'containers/App/vizier-routing';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import { ScriptContext, ScriptContextProvider } from 'context/new-script-context';
import { ScriptLoader } from 'containers/live/new-script-loader';
import { Button } from '@material-ui/core';
import { Link } from 'react-router-dom';
import { ClusterContext } from 'common/cluster-context';
import { ResultsContext, ResultsContextProvider } from 'context/results-context';

const LiveView: React.FC = () => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const { script, args, routeFor } = React.useContext(ScriptContext);
  const results = React.useContext(ResultsContext);

  if (!selectedClusterName || !script || !args) return null;

  const nextRoute = (script.id === 'px/cluster')
    ? routeFor('px/pod', { cluster: 'fooCluster', namespace: 'barNamespace', pod: 'bazPod' })
    : routeFor('px/cluster', { cluster: selectedClusterName }); // Ensure that the defaults work

  return (
    <div style={{ display: 'flex', flexFlow: 'column nowrap' }}>
      <pre style={{ overflow: 'auto', maxWidth: '100vw', maxHeight: '40vh' }}>
        {script.id}
        (
        {JSON.stringify(args)}
        )
      </pre>
      <Button
        component={Link}
        variant='contained'
        color='primary'
        to={nextRoute}
      >
        Try another route
      </Button>
      <h2>Script body</h2>
      <pre style={{ overflow: 'auto', maxWidth: '100vw', maxHeight: '40vh' }}>{JSON.stringify(script, null, 2)}</pre>
      { results.tables && (
      <>
        <h2>Results of latest script run</h2>
        <pre style={{ overflow: 'auto', maxWidth: '100vw', maxHeight: '40vh' }}>{JSON.stringify(results, null, 2)}</pre>
      </>
      )}
    </div>
  );
};

// TODO(nick,PC-917): withLiveViewContext needs a new version too that can do this. Complexity: VizierContextRouter
//  isn't just a context, it does manipulate behavior. Should it be in there? It's midway inside the stack; hard to move
const ContextualizedLiveView: React.FC = () => (
  <ScriptsContextProvider>
    <VizierContextRouter>
      <ResultsContextProvider>
        <ScriptContextProvider>
          <ScriptLoader />
          <LiveView />
        </ScriptContextProvider>
      </ResultsContextProvider>
    </VizierContextRouter>
  </ScriptsContextProvider>
);

export default ContextualizedLiveView;
