import * as React from 'react';
import { getQueryParams } from 'utils/query-params';
import { GetPxScripts } from 'utils/script-bundle';

import { ExecuteContext } from './context/execute-context';
import { ScriptContext } from './context/script-context';
import { VisContext } from './context/vis-context';
import { parseVis } from './vis';

export function useInitScriptLoader() {
  const [loaded, setLoaded] = React.useState(false);
  const script = React.useContext(ScriptContext);
  const [params, setParams] = React.useState(getQueryParams());
  const { execute } = React.useContext(ExecuteContext);
  const visSpec = React.useContext(VisContext);

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || params.script || !script || !visSpec) {
      return;
    }
    execute();
    setLoaded(true);
  }, [loaded, script, visSpec, params]);

  // This effect only runs once.
  React.useEffect(() => {
    if (!params.script) {
      return;
    }
    GetPxScripts().then((examples) => {
      for (const { title, vis, code, id } of examples) {
        if (id === params.script && code) {
          execute({
            script: code,
            vis: parseVis(vis),
            args: params,
            title,
            id,
          });
        }
      }
      // No script has been loaded if we got here, clear the script param the first effect will run.
      setParams({
        ...params,
        script: '',
      });
    });
  }, []);
}
