import { CloudClientContext } from 'context/app-context';
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
  loading: boolean;
}

const VizierGRPCClientContext = React.createContext<ContextProps>(null);

interface Props {
  passthroughEnabled: boolean;
  children: React.ReactNode;
  clusterID: string;
  clusterStatus: ClusterStatus;
}

async function newVizierClient(
  cloudClient: CloudClient, clusterID: string, passthroughEnabled: boolean,
) {
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
  const {
    children, passthroughEnabled, clusterID, clusterStatus,
  } = props;
  const cloudClient = React.useContext(CloudClientContext);
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [loading, setLoading] = React.useState(true);
  const subscriptionRef = React.useRef<Subscription>(null);
  const retryRef = React.useRef<RetryOperation>(null);
  if (!retryRef.current) {
    retryRef.current = operation({ forever: true, randomize: true });
  }

  const healthy = client && clusterStatus === 'CS_HEALTHY';

  React.useEffect(() => {
    if (!client) {
      return;
    }
    if (client.clusterID !== clusterID) {
      setLoading(true);
    }
  }, [client, clusterID]);

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
        setClient(null);
        newVizierClient(cloudClient, clusterID, passthroughEnabled).then(
          (newClient) => {
            subscriptionRef.current = newClient.health().subscribe({
              next: (status) => {
                retryRef.current.reset();
                if (status.getCode() === 0) {
                  setClient(newClient);
                  setLoading(false);
                } else {
                  setClient(null);
                }
              },
              complete: () => {
                retryRef.current.retry(new Error('stream ended'));
              },
              error: (error) => {
                setClient(null);
                retryRef.current.retry(error);
              },
            });
          },
        );
      });
    }
    return () => {
      if (subscriptionRef.current) {
        subscriptionRef.current.unsubscribe();
      }
      retryRef.current.stop();
    };
  }, [clusterID, passthroughEnabled, clusterStatus, cloudClient]);

  const context = React.useMemo(() => ({
    client,
    healthy,
    loading,
  }), [client, healthy, loading]);

  return (
    <VizierGRPCClientContext.Provider value={context}>
      {children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
