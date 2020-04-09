import * as QueryString from 'query-string';
import * as React from 'react';
import {GetPxScripts} from 'utils/script-bundle';

import {LiveContext, PlacementContextOld, ScriptContext, VegaContextOld, VisContext} from './context';
import {parseVis} from './vis';

export function useInitScriptLoader() {
  // old way
  const { executeScriptOld, setScriptsOld } = React.useContext(LiveContext);
  const vegaSpec = React.useContext(VegaContextOld);
  const placementSpec = React.useContext(PlacementContextOld);

  // both
  const [loaded, setLoaded] = React.useState(false);
  const script = React.useContext(ScriptContext);
  const queryParams = QueryString.parse(window.location.search);
  const [scriptParam, setScriptParam] = React.useState(
    typeof queryParams.script === 'string' ? queryParams.script : '');

  // new
  const { executeScript, oldLiveViewMode, setScripts } = React.useContext(LiveContext);
  const visSpec = React.useContext(VisContext);

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || scriptParam || !script) {
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
  }, [loaded, script, visSpec, placementSpec, vegaSpec]);

  React.useEffect(() => {
    if (!scriptParam) {
      return;
    }
    GetPxScripts().then((examples) => {
      for (const { title, vis, placement, code, id } of examples) {
        if (id === scriptParam && vis && placement && code) {
          setScriptsOld(code, vis, placement, { title, id });
          executeScriptOld(code);
          return;
        } else if (id === scriptParam && vis && code) {
          setScripts(code, vis, {title, id});
          executeScript(code, parseVis(vis));
        }
      }
      // No script has been loaded if we got here, clear the script param the first effect will run.
      setScriptParam('');
    });
  }, []);
}
