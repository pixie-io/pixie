import * as React from 'react';
import {getQueryParams} from 'utils/query-params';
import {GetPxScripts} from 'utils/script-bundle';

import {
    LiveContext, PlacementContextOld, ScriptContext, VegaContextOld, VisContext,
} from './context';
import {parseVis} from './vis';

export function useInitScriptLoader() {
  // old way
  const { executeScriptOld, setScriptsOld } = React.useContext(LiveContext);
  const vegaSpec = React.useContext(VegaContextOld);
  const placementSpec = React.useContext(PlacementContextOld);

  // both
  const [loaded, setLoaded] = React.useState(false);
  const script = React.useContext(ScriptContext);
  const [params, setParams] = React.useState(getQueryParams());

  // new
  const { executeScript, oldLiveViewMode, setScripts } = React.useContext(LiveContext);
  const visSpec = React.useContext(VisContext);

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || params.script || !script) {
      return;
    }
    if (oldLiveViewMode && placementSpec && vegaSpec) {
      executeScriptOld();
    } else if (!oldLiveViewMode && visSpec) {
      executeScript();
    } else {
      return;
    }

    setLoaded(true);
  }, [loaded, script, visSpec, placementSpec, vegaSpec, params]);

  // This effect only runs once.
  React.useEffect(() => {
    if (!params.script) {
      return;
    }
    GetPxScripts().then((examples) => {
      for (const { title, vis, placement, code, id } of examples) {
        if (id === params.script && vis && placement && code) {
          setScriptsOld(code, vis, placement, { title, id });
          executeScriptOld(code);
          return;
        } else if (id === params.script && vis && code) {
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
