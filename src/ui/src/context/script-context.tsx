import ClusterContext from 'common/cluster-context';
import { Script } from 'utils/script-bundle';
import { ScriptsContext } from 'containers/App/scripts-context';
import {
  LIVE_VIEW_SCRIPT_ARGS_KEY, LIVE_VIEW_SCRIPT_ID_KEY, LIVE_VIEW_PIXIE_SCRIPT_KEY,
  LIVE_VIEW_VIS_SPEC_KEY, useSessionStorage,
} from 'common/storage';
import * as React from 'react';
import { withRouter } from 'react-router';

import { parseVis, toJSON, Vis } from 'containers/live/vis';
import { argsEquals, argsForVis, Arguments } from 'utils/args-utils';
import { debounce } from 'utils/debounce';
import urlParams from 'utils/url-params';
import { SetStateFunc } from './common';
import {
  EntityURLParams, getLiveViewTitle, LiveViewPage, LiveViewPageScriptIds,
  matchLiveViewEntity, toEntityPathname,
} from '../components/live-widgets/utils/live-view-params';

interface ScriptContextProps {
  liveViewPage: LiveViewPage;

  args: Arguments;
  setArgs: (inputArgs: Arguments, newVis?: Vis, entityParamNames?: string[]) => void;
  // Resets the entity page to the default page, not an entity-centric URL.
  resetDefaultLiveViewPage: () => void;

  visJSON: string;
  vis: Vis;
  setVis: SetStateFunc<Vis>;
  setVisJSON: SetStateFunc<string>;

  pxlEditorText: string;
  visEditorText: string;
  setVisEditorText: SetStateFunc<string>;
  setPxlEditorText: SetStateFunc<string>;

  pxl: string;
  setPxl: SetStateFunc<string>;
  title: string;
  id: string;

  setScript: (vis: Vis, pxl: string, args: Arguments, id: string,
    liveViewPage?: LiveViewPage, entityParamNames?: string[]) => void;
  commitURL: () => void;
}

export const ScriptContext = React.createContext<ScriptContextProps>(null);

function emptyVis(): Vis {
  return { variables: [], widgets: [], globalFuncs: [] };
}

function getTitleOfScript(scriptId: string, scripts: Map<string, Script>): string {
  if (scripts.has(scriptId)) {
    return scripts.get(scriptId).title;
  }
  return scriptId;
}

const ScriptContextProvider = (props) => {
  const { location } = props;

  const { scripts } = React.useContext(ScriptsContext);
  const { selectedClusterName, setClusterByName } = React.useContext(ClusterContext);

  const entity = matchLiveViewEntity(location.pathname);
  const [liveViewPage, setLiveViewPage] = React.useState<LiveViewPage>(entity.page);
  const [entityParams, setEntityParams] = React.useState<EntityURLParams>(entity.params);
  const [visEditorText, setVisEditorText] = React.useState<string>();
  const [pxlEditorText, setPxlEditorText] = React.useState<string>();

  // Args that are not part of an entity.
  const [nonEntityArgs, setNonEntityArgs] = useSessionStorage<Arguments | null>(LIVE_VIEW_SCRIPT_ARGS_KEY, null);

  const [pxl, setPxl] = useSessionStorage(LIVE_VIEW_PIXIE_SCRIPT_KEY, '');
  const [id, setId] = useSessionStorage(LIVE_VIEW_SCRIPT_ID_KEY,
    entity.page === LiveViewPage.Default ? '' : LiveViewPageScriptIds[entity.page]);

  // We use a separation of visJSON states to prevent infinite update loops from happening.
  // Otherwise, an update in the editor could trigger an update to some other piece of the pie.
  // visJSON should be the incoming state to the vis editor.
  // Vis Raw is the outgoing state from the vis editor.
  const [visJSON, setVisJSONBase] = useSessionStorage<string>(LIVE_VIEW_VIS_SPEC_KEY);
  const [vis, setVisBase] = React.useState(() => {
    const parsed = parseVis(visJSON);
    if (parsed) {
      return parsed;
    }
    return emptyVis();
  });

  // args are the combination of entity and not entity params.
  const args = React.useMemo(
    () => argsForVis(vis, { ...nonEntityArgs, ...entityParams }),
    [vis, nonEntityArgs, entityParams],
  );

  // title is dependent on whether or not we are in an entity page.
  const title = React.useMemo(() => {
    const newTitle = getLiveViewTitle(getTitleOfScript(id, scripts), liveViewPage, entityParams);
    document.querySelector('title').textContent = newTitle;
    return newTitle;
  }, [liveViewPage, scripts, id, entityParams]);

  // Logic to set cluster

  React.useEffect(() => {
    if (entity.clusterName && entity.clusterName !== selectedClusterName) {
      setClusterByName(entity.clusterName);
    }
  }, []);

  // Logic to set url params when location changes

  React.useEffect(() => {
    if (location.pathname !== '/live') {
      urlParams.triggerOnChange();
    }
  }, [location]);

  // Logic to update entity paths when live view page or cluster changes.

  React.useEffect(() => {
    const entityURL = {
      clusterName: selectedClusterName,
      page: liveViewPage,
      params: entityParams,
    };
    urlParams.setPathname(toEntityPathname(entityURL));
  }, [selectedClusterName, liveViewPage, entityParams]);

  React.useEffect(() => {
    const scriptID = liveViewPage === LiveViewPage.Default ? id : '';
    urlParams.setScript(scriptID, /* diff */'');
  }, [liveViewPage, id]);

  // Used to reset to the "default" page if the script is edited.
  const resetDefaultLiveViewPage = () => {
    setNonEntityArgs(argsForVis(vis, { ...nonEntityArgs, ...entityParams }));
    setEntityParams({});
    setLiveViewPage(LiveViewPage.Default);
  };

  // Logic to update arguments (which are a combo of entity params and normal args)

  // TODO(nserrino): Add unit test for this function.
  const setArgs = (inputArgs: Arguments, newVis?: Vis, entityParamNames?: string[]) => {
    // newVis can correctly be null for scripts that have no vis spec,
    // so we check to see if newVis is undefined or not to figure out whether or not
    // it has been passed in as an argument to setArgs.
    const parsedArgs = argsForVis(newVis !== undefined ? newVis : vis, inputArgs);
    let entityNames: Set<string>;

    if (entityParamNames == null) {
      entityNames = new Set(Object.keys(parsedArgs)
        .filter((argName) => typeof entityParams[argName] === 'string'));
    } else {
      entityNames = new Set(entityParamNames);
    }

    const inputEntityArgs = Object.keys(parsedArgs)
      .filter((argName) => entityNames.has(argName))
      .reduce((obj, argName) => {
        obj[argName] = parsedArgs[argName];
        return obj;
      }, {});

    if (!argsEquals(entityParams, inputEntityArgs)) {
      setEntityParams(inputEntityArgs);
    }

    const inputNonEntityArgs = Object.keys(parsedArgs)
      .filter((argName) => !entityNames.has(argName))
      .reduce((obj, argName) => {
        obj[argName] = parsedArgs[argName];
        return obj;
      }, {});

    if (!argsEquals(nonEntityArgs, inputNonEntityArgs)) {
      setNonEntityArgs(inputNonEntityArgs);
    }
  };

  React.useEffect(() => {
    if (nonEntityArgs) {
      urlParams.setArgs(nonEntityArgs);
    }
  }, [nonEntityArgs]);

  // Logic to update vis spec.

  // Note that this function does not update args, so it should only be called
  // when variables will not be modified (such as for layouts).
  const setVis = (newVis: Vis) => {
    if (!newVis) {
      newVis = emptyVis();
    }
    setVisJSONBase(toJSON(newVis));
    setVisBase(newVis);
  };

  const setVisDebounce = React.useRef(debounce((newJSON: string) => {
    const parsed = parseVis(newJSON);
    if (parsed) {
      setVisBase(parsed);
      setArgs(args, parsed);
    }
  }, 2000));

  const setVisJSON = (newJSON: string) => {
    setVisJSONBase(newJSON);
    setVisDebounce.current(newJSON);
  };

  // Logic to update the full script

  const setScript = (newVis: Vis, newPxl: string, newArgs: Arguments, newID: string,
    newLiveViewPage?: LiveViewPage, entityParamNames?: string[]) => {
    setVis(newVis);
    setPxl(newPxl);
    if (entityParamNames) {
      setArgs(newArgs, newVis, entityParamNames);
    } else {
      setArgs(newArgs, newVis);
    }
    setId(newID);
    if (newLiveViewPage != null) {
      setLiveViewPage(newLiveViewPage);
    }
  };

  // Logic to commit the URL to the history.
  const commitURL = () => {
    // Only show the script as a query arg when we are not on an entity page.
    const scriptId = liveViewPage === LiveViewPage.Default ? (id || '') : '';
    urlParams.commitAll(scriptId, '', nonEntityArgs);
  };

  return (
    <ScriptContext.Provider
      value={{
        liveViewPage,
        args,
        setArgs,
        resetDefaultLiveViewPage,
        pxlEditorText,
        visEditorText,
        setPxlEditorText,
        setVisEditorText,
        vis,
        setVis,
        visJSON,
        setVisJSON,
        pxl,
        setPxl,
        title,
        id,
        setScript,
        commitURL,
      }}
    >
      {props.children}
    </ScriptContext.Provider>
  );
};

export default withRouter(ScriptContextProvider);
