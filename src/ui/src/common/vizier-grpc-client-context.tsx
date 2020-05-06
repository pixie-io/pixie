import * as React from 'react';
import { Subscription } from 'rxjs';
import { debounce } from 'utils/debounce';
import { isDev } from 'utils/env';

import { CloudClient } from './cloud-gql-client';
import { VizierGRPCClient } from './vizier-grpc-client';

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

interface Props {
  cloudClient: CloudClient;
  passthroughEnabled: boolean;
  children: React.ReactNode;
  clusterID: string;
  loadingScreen: React.ReactNode;
  vizierVersion: string;
}

export type VizierConnectionStatus = 'healthy' | 'unhealthy' | 'disconnected';

async function newVizierClient(
  cloudClient: CloudClient, clusterID: string, passthroughEnabled: boolean, vizierVersion: string) {

  const { ipAddress, token } = await cloudClient.getClusterConnection(true);
  let address = ipAddress;
  if (passthroughEnabled) {
    // If cloud is running in dev mode, automatically direct to Envoy's port, since there is
    // no GCLB to redirect for us in dev.
    address = window.location.origin + (isDev() ? ':4444' : '');
  }
  return new VizierGRPCClient(address, token, clusterID, passthroughEnabled, vizierVersion);
}

export const VizierGRPCClientProvider = (props: Props) => {
  const { cloudClient, children, passthroughEnabled, clusterID, vizierVersion } = props;
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [connectionStatus, setConnectionStatus] = React.useState<VizierConnectionStatus>('disconnected');
  const [loaded, setLoaded] = React.useState(false);
  const [subscription, setSubscription] = React.useState<Subscription>(null);

  const newClient = () => {
    if (subscription) {
      subscription.unsubscribe();
    }
    newVizierClient(cloudClient, clusterID, passthroughEnabled, vizierVersion).then(setClient);
  };
  const reconnect = () => {
    setSubscription(client.health().subscribe({
      next: (status) => {
        if (status.getCode() === 0) {
          setConnectionStatus('healthy');
          setLoaded(true);
        } else {
          setConnectionStatus('unhealthy');
        }
      },
      complete: debounce(reconnect, 2000),
      error: () => {
        setConnectionStatus('disconnected');
        newClient();
      },
    }));
  };

  React.useEffect(() => {
    newClient();
  }, [clusterID, passthroughEnabled]);

  React.useEffect(() => {
    if (client) {
      reconnect();
    }
  }, [client]);

  return (
    <VizierGRPCClientContext.Provider value={connectionStatus === 'disconnected' ? null : client}>
      {!loaded ? props.loadingScreen : children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
