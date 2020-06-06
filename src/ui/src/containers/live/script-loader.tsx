import { ScriptsContext } from 'containers/App/scripts-context';
import * as React from 'react';
import { argsForVis } from 'utils/args-utils';
import QueryParams from 'utils/query-params';

import { ExecuteContext } from './context/execute-context';
import { ScriptContext } from './context/script-context';
import { VisContext } from './context/vis-context';
import { parseVis } from './vis';

export function ScriptLoader() {
  const [loaded, setLoaded] = React.useState(false);
  const { promise: scriptPromise, scripts } = React.useContext(ScriptsContext);
  const script = React.useContext(ScriptContext);
  const [params, setParams] = React.useState({
    scriptId: QueryParams.scriptId,
    args: QueryParams.args,
  });
  const { execute } = React.useContext(ExecuteContext);
  const visSpec = React.useContext(VisContext);
  const ref = React.useRef({
    scripts,
    execute,
  });

  ref.current.scripts = scripts;
  ref.current.execute = execute;

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || params.scriptId || !script || !visSpec) {
      return;
    }
    execute();
    setLoaded(true);
  }, [loaded, script, visSpec, params]);

  // This effect only runs once.
  React.useEffect(() => {
    if (!params.scriptId) {
      return;
    }
    scriptPromise.then((examples) => {
      for (const { title, vis, code, id } of examples) {
        if (id === params.scriptId && code) {
          const parsedVis = parseVis(vis);
          const args = argsForVis(parsedVis, params.args, id);
          execute({
            script: code,
            vis: parsedVis,
            args,
            title,
            id,
          });
          return;
        }
      }
      // No script has been loaded if we got here, clear the script param the first effect will run.
      setParams({ scriptId: '', args: {} });
    });
  }, []);

  React.useEffect(() => {
    const subscription = QueryParams.onChange.subscribe(({ scriptId, args }) => {
      for (const { title, vis, code, id } of ref.current.scripts) {
        if (id === scriptId && code) {
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
        }
      }
    });
    return () => {
      subscription.unsubscribe();
    };
  }, [])
  return null;
}
