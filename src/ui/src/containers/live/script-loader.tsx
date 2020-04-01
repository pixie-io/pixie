import * as QueryString from 'query-string';
import * as React from 'react';
import {GetPxScripts} from 'utils/script-bundle';

import {LiveContext, PlacementContext, ScriptContext, VegaContext} from './context';

export function useInitScriptLoader() {
  const { setScripts } = React.useContext(LiveContext);
  const { executeScript } = React.useContext(LiveContext);
  const script = React.useContext(ScriptContext);
  const vega = React.useContext(VegaContext);
  const place = React.useContext(PlacementContext);
  const [loaded, setLoaded] = React.useState(false);

  const queryParams = QueryString.parse(window.location.search);
  const [scriptParam, setScriptParam] = React.useState(
    typeof queryParams.script === 'string' ? queryParams.script : '');

  // Execute the default scripts if script is not set in the query params.
  React.useEffect(() => {
    if (loaded || scriptParam || !script || !vega || !place) {
      return;
    }
    executeScript();
    setLoaded(true);
  }, [loaded, script, vega, place]);

  // Load and execute the script if query params is set.
  React.useEffect(() => {
    if (!scriptParam) {
      return;
    }
    GetPxScripts().then((examples) => {
      for (const { title, vis, placement, code } of examples) {
        if (title === scriptParam && vis && placement && code) {
          setScripts(code, vis, placement, title);
          executeScript(code);
          return;
        }
      }
      // No script has been loaded if we got here, clear the script param the first effect will run.
      setScriptParam('');
    });
  }, []);
}
