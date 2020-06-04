import { CloudClientContext } from 'containers/App/context';
import { ClusterInstructions, DeployInstructions } from 'containers/vizier/deploy-instructions';
import * as React from 'react';
import { operation, RetryOperation } from 'retry';
import { Subscription } from 'rxjs';
import { isDev } from 'utils/env';

import { CloudClient } from './cloud-gql-client';
import { VizierGRPCClient } from './vizier-grpc-client';

export const CLUSTER_STATUS_UNKNOWN = 'CS_UNKNOWN';
export const CLUSTER_STATUS_HEALTHY = 'CS_HEALTHY';
export const CLUSTER_STATUS_UNHEALTHY = 'CS_UNHEALTHY';
export const CLUSTER_STATUS_DISCONNECTED = 'CS_DISCONNECTED';
export const CLUSTER_STATUS_UPDATING = 'CS_UPDATING';
export const CLUSTER_STATUS_CONNECTED = 'CS_CONNECTED';
export const CLUSTER_STATUS_UPDATE_FAILED = 'CS_UPDATE_FAILED';

export type ClusterStatus =
  typeof CLUSTER_STATUS_UNKNOWN |
  typeof CLUSTER_STATUS_HEALTHY |
  typeof CLUSTER_STATUS_UNHEALTHY |
  typeof CLUSTER_STATUS_DISCONNECTED |
  typeof CLUSTER_STATUS_UPDATING |
  typeof CLUSTER_STATUS_CONNECTED |
  typeof CLUSTER_STATUS_UPDATE_FAILED;

interface ContextProps {
  client: VizierGRPCClient | null;
  healthy: boolean;
}

const VizierGRPCClientContext = React.createContext<ContextProps>(null);

interface Props {
  passthroughEnabled: boolean;
  children: React.ReactNode;
  clusterID: string;
  clusterStatus: ClusterStatus;
}

async function newVizierClient(
  cloudClient: CloudClient, clusterID: string, passthroughEnabled: boolean) {

  const { ipAddress, token } = await cloudClient.getClusterConnection(clusterID, true);
  let address = ipAddress;
  if (passthroughEnabled) {
    // If cloud is running in dev mode, automatically direct to Envoy's port, since there is
    // no GCLB to redirect for us in dev.
    address = window.location.origin + (isDev() ? ':4444' : '');
  }
  return new VizierGRPCClient(address, token, clusterID, passthroughEnabled);
}

export const VizierGRPCClientProvider = (props: Props) => {
  const { children, passthroughEnabled, clusterID, clusterStatus } = props;
  const cloudClient = React.useContext(CloudClientContext);
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [connected, setConnected] = React.useState<boolean>(false);
  const [loaded, setLoaded] = React.useState(false);
  const subscriptionRef = React.useRef<Subscription>(null);
  const retryRef = React.useRef<RetryOperation>(null);
  if (!retryRef.current) {
    retryRef.current = operation({ forever: true, randomize: true });
  }

  const healthy = clusterStatus === 'CS_HEALTHY' && connected;

  React.useEffect(() => {
    if (clusterStatus !== 'CS_HEALTHY') {
      retryRef.current.stop();
      if (subscriptionRef.current) {
        subscriptionRef.current.unsubscribe();
        subscriptionRef.current = null;
      }
    } else {
      // Cluster is healthy
      retryRef.current.reset();
      retryRef.current.attempt(() => {
        if (subscriptionRef.current) {
          subscriptionRef.current.unsubscribe();
        }
        newVizierClient(cloudClient, clusterID, passthroughEnabled).then(
          (client) => {
            setClient(client);
            subscriptionRef.current = client.health().subscribe({
              next: (status) => {
                retryRef.current.reset();
                if (status.getCode() === 0) {
                  setConnected(true);
                  setLoaded(true);
                } else {
                  setConnected(false);
                }
              },
              complete: () => {
                retryRef.current.retry(new Error('stream ended'));
              },
              error: (error) => {
                setConnected(false);
                retryRef.current.retry(error);
              },
            });
          });
      });
    }
    return () => {
      if (subscriptionRef.current) {
        subscriptionRef.current.unsubscribe();
      }
      retryRef.current.stop();
    };

  }, [clusterID, passthroughEnabled, clusterStatus]);

  const context = React.useMemo(() => {
    return {
      client,
      healthy,
    };
  }, [client, healthy])

  if (!loaded && clusterStatus === 'CS_DISCONNECTED') {
    return <DeployInstructions />;
  }

  if (!loaded) {
    return <ClusterInstructions message='Connecting to cluster...' />;
  }

  return (
    <VizierGRPCClientContext.Provider value={context}>
      {children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
