import * as React from 'react';

interface ClusterContextProps {
  selectedCluster: string;
  selectedClusterName: string;
  selectedClusterPrettyName: string;
  selectedClusterUID: string;
  setCluster: (id: string) => void;
  setClusterByName: (name: string) => void;
}

export default React.createContext<ClusterContextProps>(null);
