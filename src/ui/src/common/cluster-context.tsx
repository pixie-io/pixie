import * as React from 'react';

export interface ClusterContextProps {
  selectedCluster: string;
  selectedClusterName: string;
  selectedClusterPrettyName: string;
  selectedClusterUID: string;
  setCluster: (id: string) => void;
  setClusterByName: (name: string) => void;
}

export const ClusterContext = React.createContext<ClusterContextProps>(null);
