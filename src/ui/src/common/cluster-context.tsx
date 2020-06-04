import * as React from 'react';

interface ClusterContextProps {
  selectedCluster: string;
  setCluster: (id: string) => void;
}

export default React.createContext<ClusterContextProps>(null);
