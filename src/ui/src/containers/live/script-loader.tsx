import * as QueryString from 'query-string';
import * as React from 'react';
import {GetPxScripts} from 'utils/script-bundle';

import {LiveContext, PlacementContextOld, ScriptContext, VegaContextOld} from './context';

export function useInitScriptLoader() {
  const { setScriptsOld } = React.useContext(LiveContext);
  const { executeScriptOld } = React.useContext(LiveContext);
  const script = React.useContext(ScriptContext);
  const vega = React.useContext(VegaContextOld);
  const place = React.useContext(PlacementContextOld);
  const [loaded, setLoaded] = React.useState(false);

  const queryParams = QueryString.parse(window.location.search);
  const [scriptParam, setScriptParam] = React.useState(
    typeof queryParams.script === 'string' ? queryParams.script : '');

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || scriptParam || !script || !vega || !place) {
      return;
    }
    executeScriptOld();
    setLoaded(true);
  }, [loaded, script, vega, place]);

  // Load and execute the script if query params is set.
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
        }
      }
      // No script has been loaded if we got here, clear the script param the first effect will run.
      setScriptParam('');
    });
  }, []);
}
