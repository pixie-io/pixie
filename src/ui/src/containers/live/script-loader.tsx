import { ScriptsContext } from 'containers/App/scripts-context';
import * as React from 'react';
import { argsForVis } from 'utils/args-utils';
import urlParams from 'utils/url-params';

import { ExecuteContext } from '../../context/execute-context';
import { ScriptContext } from '../../context/script-context';
import { parseVis } from './vis';
import {
  LiveViewPage,
  LiveViewPageScriptIds,
  matchLiveViewEntity,
} from '../../components/live-widgets/utils/live-view-params';

type LoadScriptState = 'unloaded' | 'url-loaded' | 'url-skipped' | 'context-loaded';

export function ScriptLoader() {
  const [loadState, setLoadState] = React.useState<LoadScriptState>('unloaded');
  const { promise: scriptPromise } = React.useContext(ScriptsContext);
  const { pxl, vis } = React.useContext(ScriptContext);

  const { execute } = React.useContext(ExecuteContext);
  const ref = React.useRef({
    urlLoaded: false,
    execute,
  });

  ref.current.execute = execute;

  // Execute the default scripts if script was not loaded from the URL.
  React.useEffect(() => {
    if (loadState === 'url-skipped') {
      if (pxl && vis) {
        execute();
        setLoadState('context-loaded');
      }
    }
  }, [loadState, pxl, vis]);

  React.useEffect(() => {
    // TODO(nserrino): refactor this legacy code to reduce duplication with ScriptContext.
    // (matchLiveViewEntity et al).
    const subscription = urlParams.onChange.subscribe(({ scriptId, pathname, args }) => {
      scriptPromise.then((scripts) => {
        const entity = matchLiveViewEntity(pathname);
        const id = entity.page === LiveViewPage.Default ? scriptId : LiveViewPageScriptIds[entity.page];

        if (!scripts.has(id)) {
          setLoadState((state) => {
            if (state !== 'unloaded') {
              return state;
            }
            return 'url-skipped';
          });
          return;
        }

        const { vis, code } = scripts.get(id);

        const parsedVis = parseVis(vis);
        const parsedArgs = argsForVis(parsedVis, { ...args, ...entity.params }, id);
        ref.current.execute({
          liveViewPage: entity.page,
          pxl: code,
          vis: parsedVis,
          args: parsedArgs,
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
  }, []);
  return null;
}
