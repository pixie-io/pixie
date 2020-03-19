import * as React from 'react';
import {debounce} from 'utils/debounce';

import {isDev} from 'utils/env';
import {CloudClient} from './cloud-gql-client';
import {VizierGRPCClient} from './vizier-grpc-client';

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

interface Props {
  cloudClient: CloudClient;
  passthroughEnabled: boolean;
  children: React.ReactNode;
  clusterID: string;
}

export type VizierConnectionStatus = 'healthy' | 'unhealthy' | 'disconnected';

async function newVizierClient(cloudClient: CloudClient, clusterID: string, passthroughEnabled: boolean) {
  const { ipAddress, token } = await cloudClient.getClusterConnection(true);
  let address = ipAddress;
  if (passthroughEnabled) {
    // If cloud is running in dev mode, automatically direct to Envoy's port, since there is
    // no GCLB to redirect for us in dev.
    address = window.location.origin + (isDev() ? ':4444' : '');
  }
  return new VizierGRPCClient(address, token, clusterID, passthroughEnabled);
}

export const VizierGRPCClientProvider = (props: Props) => {
  const { cloudClient, children, passthroughEnabled, clusterID} = props;
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [connectionStatus, setConnectionStatus] = React.useState<VizierConnectionStatus>('disconnected');

  const newClient = () => newVizierClient(cloudClient, clusterID, passthroughEnabled).then(setClient);
  const reconnect = () => {
    client.health().subscribe({
      next: (status) => {
        if (status.getCode() === 0) {
          setConnectionStatus('healthy');
        } else {
          setConnectionStatus('unhealthy');
        }
      },
      complete: debounce(reconnect, 2000),
      error: () => {
        setConnectionStatus('disconnected');
        newClient();
      },
    });
  };

  React.useEffect(() => {
    if (!client) {
      newClient();
      return;
    }
    reconnect();
  }, [client]);

  return (
    <VizierGRPCClientContext.Provider value={connectionStatus === 'disconnected' ? null : client}>
      {children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
