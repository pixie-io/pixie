import { CloudClientContext } from 'containers/App/context';
import * as React from 'react';
import { operation, RetryOperation } from 'retry';
import { Subscription } from 'rxjs';
import { debounce } from 'utils/debounce';
import { isDev } from 'utils/env';

import { CloudClient } from './cloud-gql-client';
import { VizierGRPCClient } from './vizier-grpc-client';

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

interface Props {
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
  const { children, passthroughEnabled, clusterID, vizierVersion } = props;
  const cloudClient = React.useContext(CloudClientContext);
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [connectionStatus, setConnectionStatus] = React.useState<VizierConnectionStatus>('disconnected');
  const [loaded, setLoaded] = React.useState(false);
  const subscriptionRef = React.useRef<Subscription>(null);
  const retryRef = React.useRef<RetryOperation>(null);
  if (!retryRef.current) {
    retryRef.current = operation({ forever: true, randomize: true });
  }

  React.useEffect(() => {
    retryRef.current.reset();
    retryRef.current.attempt(() => {
      if (subscriptionRef.current) {
        subscriptionRef.current.unsubscribe();
      }
      newVizierClient(cloudClient, clusterID, passthroughEnabled, vizierVersion).then(
        (client) => {
          setClient(client);
          subscriptionRef.current = client.health().subscribe({
            next: (status) => {
              retryRef.current.reset();
              if (status.getCode() === 0) {
                setConnectionStatus('healthy');
                setLoaded(true);
              } else {
                setConnectionStatus('unhealthy');
              }
            },
            complete: () => {
              retryRef.current.retry(new Error('stream ended'));
            },
            error: (error) => {
              setConnectionStatus('disconnected');
              retryRef.current.retry(error);
            },
          });
        });
    });
    return () => {
      if (subscriptionRef.current) {
        subscriptionRef.current.unsubscribe();
      }
    };
  }, [clusterID, passthroughEnabled]);

  return (
    <VizierGRPCClientContext.Provider value={connectionStatus === 'disconnected' ? null : client}>
      {!loaded ? props.loadingScreen : children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
