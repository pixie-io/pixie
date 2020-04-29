import * as React from 'react';
import {getQueryParams} from 'utils/query-params';
import {GetPxScripts} from 'utils/script-bundle';

import {LiveContext, ScriptContext, VisContext} from './context';
import {parseVis} from './vis';

export function useInitScriptLoader() {
  const [loaded, setLoaded] = React.useState(false);
  const script = React.useContext(ScriptContext);
  const [params, setParams] = React.useState(getQueryParams());
  const { executeScript, setScripts } = React.useContext(LiveContext);
  const visSpec = React.useContext(VisContext);

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || params.script || !script || !visSpec) {
      return;
    }
    executeScript();
    setLoaded(true);
  }, [loaded, script, visSpec, params]);

  // This effect only runs once.
  React.useEffect(() => {
    if (!params.script) {
      return;
    }
    GetPxScripts().then((examples) => {
      for (const { title, vis, code, id } of examples) {
        if (id === params.script && vis && code) {
          setScripts(code, vis, { title, id }, params);
          executeScript(code, parseVis(vis), params);
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
