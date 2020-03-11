import * as React from 'react';

import {CloudClient} from './cloud-gql-client';
import {VizierGRPCClient} from './vizier-grpc-client';

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

interface Props {
  cloudClient: CloudClient;
  children: React.ReactNode;
}

export const VizierGRPCClientProvider = (props: Props) => {
  const { cloudClient, children } = props;
  const [client, setClient] = React.useState(null);

  React.useEffect(() => {
    cloudClient.getClusterConnection(true).then(({ ipAddress, token }) => {
      setClient(new VizierGRPCClient(ipAddress, token));
    });
  }, []);

  return (
    <VizierGRPCClientContext.Provider value={client}>
      {children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
