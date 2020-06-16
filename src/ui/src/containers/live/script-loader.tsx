import { ScriptsContext } from 'containers/App/scripts-context';
import * as React from 'react';
import { argsForVis } from 'utils/args-utils';
import urlParams from 'utils/url-params';

import { ExecuteContext } from './context/execute-context';
import { RouteContext } from './context/route-context';
import { ScriptContext } from './context/script-context';
import { VisContext } from './context/vis-context';
import { parseVis } from './vis';
import { LiveViewPage, LiveViewPageScriptIds } from './utils/live-view-params';

type LoadScriptState = 'unloaded' | 'url-loaded' | 'url-skipped' | 'context-loaded';

export function ScriptLoader() {
  const [loadState, setLoadState] = React.useState<LoadScriptState>('unloaded');
  const { promise: scriptPromise } = React.useContext(ScriptsContext);
  const { script } = React.useContext(ScriptContext);
  const [params, setParams] = React.useState({
    scriptId: urlParams.scriptId,
    args: urlParams.args,
  });

  const { liveViewPage } = React.useContext(RouteContext);
  const { execute } = React.useContext(ExecuteContext);
  const visSpec = React.useContext(VisContext);
  const ref = React.useRef({
    urlLoaded: false,
    execute,
  });

  ref.current.execute = execute;

  // Execute the default scripts if script was not loaded from the URL.
  React.useEffect(() => {
    if (loadState === 'url-skipped') {
      if (script && visSpec) {
        execute();
        setLoadState('context-loaded');
      }
    }
  }, [loadState, script, visSpec]);

  React.useEffect(() => {
    const subscription = urlParams.onChange.subscribe(({ scriptId, args }) => {
      scriptPromise.then((scripts) => {
        const id = liveViewPage === LiveViewPage.Default ? scriptId : LiveViewPageScriptIds[liveViewPage];

        if (!scripts.has(id)) {
          setLoadState((state) => {
            if (state !== 'unloaded') {
              return state;
            }
            return 'url-skipped';
          });
          return;
        }

        const { title, vis, code } = scripts.get(id);

        const parsedVis = parseVis(vis);
        const parsedArgs = argsForVis(parsedVis, args, id);
        ref.current.execute({
          script: code,
          vis: parsedVis,
          args: parsedArgs,
          title,
          id,
          skipURLUpdate: true,
        });
        setLoadState((state) => {
          if (state !== 'unloaded') {
            return state;
          }
          return 'url-loaded';
        });
      });
    });
    return () => {
      subscription.unsubscribe();
    };
  }, [])
  return null;
}
