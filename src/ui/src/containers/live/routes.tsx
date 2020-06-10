import ClusterContext from 'common/cluster-context';
import * as H from 'history';
import * as React from 'react';
import { withRouter } from 'react-router';

import { LiveViewURLParams } from './utils/url-params';

interface LiveViewRoutesProps {
  location: H.Location;
  history: H.History;
}

const LiveViewRoutes = (props: LiveViewRoutesProps) => {
  const { location, history } = props;
  const [prevPath, setPrevPath] = React.useState('');
  const { selectedClusterName, setClusterByName } = React.useContext(ClusterContext);
  const curPath = location.pathname;
  const paramsRef = React.useRef(new LiveViewURLParams(selectedClusterName));
  const params = paramsRef.current;

  if (prevPath !== curPath) {
    if (params.matchLiveViewParams(curPath)) {
      setClusterByName(params.clusterName);
    }
    setPrevPath(curPath);
  }

  React.useEffect(() => {
    params.clusterName = selectedClusterName;
    history.push(`${params.toURL()}${window.location.search}`);
  }, [selectedClusterName]);

  return null;
};

export default withRouter(LiveViewRoutes);
