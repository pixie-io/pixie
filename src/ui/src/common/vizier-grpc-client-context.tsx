import * as React from 'react';

import {CloudClient} from './cloud-gql-client';
import {VizierGRPCClient} from './vizier-grpc-client';

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

interface Props {
  cloudClient: CloudClient;
  children: React.ReactNode;
}

export type VizierConnectionStatus = 'healthy' | 'unhealthy' | 'disconnected';

async function newVizierClient(cloudClient: CloudClient) {
  const { ipAddress, token } = await cloudClient.getClusterConnection(true);
  return new VizierGRPCClient(ipAddress, token);
}

export const VizierGRPCClientProvider = (props: Props) => {
  const { cloudClient, children } = props;
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [connectionStatus, setConnectionStatus] = React.useState<VizierConnectionStatus>('disconnected');

  const reconnect = () => newVizierClient(cloudClient).then(setClient);

  React.useEffect(() => {
    if (!client) {
      reconnect();
      return;
    }
    client.health().subscribe({
      next: (status) => {
        if (status.getCode() === 0) {
          setConnectionStatus('healthy');
        } else {
          setConnectionStatus('unhealthy');
        }
      },
      complete: reconnect,
      error: () => {
        setConnectionStatus('disconnected');
        reconnect();
      },
    });
  }, [client]);

  return (
    <VizierGRPCClientContext.Provider value={connectionStatus === 'disconnected' ? null : client}>
      {children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
