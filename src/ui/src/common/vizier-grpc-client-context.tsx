import * as React from 'react';

import {getClusterConnection} from './cloud-gql-client';
import {VizierGRPCClient} from './vizier-grpc-client';

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

export const VizierGRPCClientProvider: React.FC = (props) => {
  const [client, setClient] = React.useState(null);

  React.useEffect(() => {
    getClusterConnection(true).then(({ ipAddress, token }) => {
      setClient(new VizierGRPCClient(ipAddress, token));
    });
  }, []);

  return (
    <VizierGRPCClientContext.Provider value={client}>
      {props.children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
