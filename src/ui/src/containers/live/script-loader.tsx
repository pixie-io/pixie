import { ScriptsContext } from 'containers/App/scripts-context';
import * as React from 'react';
import { argsForVis } from 'utils/args-utils';
import urlParams from 'utils/url-params';
import { ContainsMutation } from 'utils/pxl';

import { ScriptContext } from 'context/script-context';
import { ResultsContext } from 'context/results-context';
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
  const {
    pxl, vis, args, id, liveViewPage, setScript, execute,
  } = React.useContext(ScriptContext);

  const { clearResults } = React.useContext(ResultsContext);
  const ref = React.useRef({
    urlLoaded: false,
    execute,
  });

  ref.current.execute = execute;

  // Execute the default scripts if script was not loaded from the URL.
  React.useEffect(() => {
    if (loadState === 'url-skipped') {
      if (pxl && vis) {
        execute({
          pxl,
          vis,
          args,
          id,
          liveViewPage,
        });
        setLoadState('context-loaded');
      }
    }
  }, [execute, loadState, pxl, vis]);

  React.useEffect(() => {
    // TODO(nserrino): refactor this legacy code to reduce duplication with ScriptContext.
    // (matchLiveViewEntity et al).
    const subscription = urlParams.onChange.subscribe((urlInfo) => {
      const { pathname } = urlInfo;
      const urlArgs = urlInfo.args;
      const urlScriptId = urlInfo.scriptId;
      scriptPromise.then((scripts) => {
        const entity = matchLiveViewEntity(pathname);
        const selectedId = entity.page === LiveViewPage.Default ? urlScriptId : LiveViewPageScriptIds.get(entity.page);

        if (!scripts.has(selectedId)) {
          setLoadState((state) => {
            if (state !== 'unloaded') {
              return state;
            }
            return 'url-skipped';
          });
          return;
        }

        const script = scripts.get(selectedId);
        const parsedVis = parseVis(script.vis);
        const parsedArgs = argsForVis(parsedVis, { ...urlArgs, ...entity.params }, selectedId);
        const execArgs = {
          liveViewPage: entity.page,
          pxl: script.code,
          vis: parsedVis,
          args: parsedArgs,
          id: selectedId,
          skipURLUpdate: true,
        };
        clearResults();
        setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.liveViewPage);
        if (!ContainsMutation(execArgs.pxl)) {
          ref.current.execute(execArgs);
        }
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
  }, [scriptPromise]);
  return null;
}
