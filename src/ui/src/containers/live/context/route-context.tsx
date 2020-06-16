import ClusterContext from 'common/cluster-context';
import * as React from 'react';
import { withRouter } from 'react-router';

import { SetStateFunc } from './common';
import urlParams from 'utils/url-params';
import { EntityURLParams, LiveViewPage, matchLiveViewEntity, toEntityPathname } from '../utils/live-view-params';

interface RouteContextProps {
  liveViewPage: LiveViewPage;
  setLiveViewPage: SetStateFunc<LiveViewPage>;
  entityParams: EntityURLParams;
  setEntityParams: SetStateFunc<EntityURLParams>;
}

export const RouteContext = React.createContext<RouteContextProps>(null);

const RouteContextProvider = (props) => {
  const { location } = props;
  const { selectedClusterName, setClusterByName } = React.useContext(ClusterContext);

  const entity = matchLiveViewEntity(location.pathname);
  const [ liveViewPage, setLiveViewPage ] = React.useState<LiveViewPage>(entity.page);
  const [ entityParams, setEntityParams ] = React.useState<EntityURLParams>(entity.params);

  React.useEffect(() => {
    if (entity.clusterName && entity.clusterName !== selectedClusterName) {
      setClusterByName(entity.clusterName);
    }
  }, []);

  React.useEffect(() => {
    const entityURL = {
      clusterName: selectedClusterName,
      page: liveViewPage,
      params: entityParams,
    };
    urlParams.setPathname(toEntityPathname(entityURL));
  }, [selectedClusterName, liveViewPage, entityParams]);

  return (
    <RouteContext.Provider
      value={{
        liveViewPage,
        setLiveViewPage,
        entityParams,
        setEntityParams,
      }}>
      {props.children}
    </RouteContext.Provider >
  );
};

export default withRouter(RouteContextProvider);
