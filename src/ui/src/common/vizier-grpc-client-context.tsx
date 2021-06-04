/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';
import { operation } from 'retry';
import { isDev } from 'utils/env';

import { PixieAPIContext } from 'api';
import { VizierGRPCClient, CloudClient, GQLClusterStatus as ClusterStatus } from '@pixie-labs/api';

export interface VizierGRPCClientContextProps {
  client: VizierGRPCClient | null;
  healthy: boolean;
  loading: boolean;
  clusterStatus: ClusterStatus;
}

export const VizierGRPCClientContext = React.createContext<VizierGRPCClientContextProps>(null);

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

export const VizierGRPCClientProvider: React.FC<Props> = ({
  children, passthroughEnabled, clusterID, clusterStatus,
}) => {
  // TEMPORARY workaround so we can make the architectural changes needed to fix this properly.
  // The issue is that CloudClient gets imported from dist here, but the object we're referencing was imported from src.
  // Although it's the exact same interface, TypeScript sees that it came from two different sources and that it has
  // private properties. It then assumes this is a problem and the interfaces might differ, so it's a compilation error.
  // We suppress this (for now) to work around it.
  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
  // @ts-ignore
  const cloudClient: CloudClient = React.useContext(PixieAPIContext).getCloudGQLClientForAdapterLibrary();
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [loading, setLoading] = React.useState(true);

  const healthy = client && clusterStatus === ClusterStatus.CS_HEALTHY;

  React.useEffect(() => {
    // Everytime the clusterID changes, we enter a loading state until we
    // receive a healthy status.
    setLoading(true);
  }, [clusterID]);

  React.useEffect(() => {
    let currentSubscription = null;
    let subscriptionPromise = Promise.resolve();
    const retryOp = operation({ forever: true, randomize: true });
    // TODO might need to remove this.
    if (clusterStatus !== ClusterStatus.CS_HEALTHY) {
      retryOp.stop();
      if (currentSubscription) {
        currentSubscription.unsubscribe();
        currentSubscription = null;
      }
    } else {
      // Cluster is healthy
      retryOp.reset();
      retryOp.attempt(() => {
        if (currentSubscription) {
          currentSubscription.unsubscribe();
        }
        setClient(null);
        subscriptionPromise = newVizierClient(cloudClient, clusterID, passthroughEnabled).then(
          (newClient) => {
            currentSubscription = newClient.health().subscribe({
              next: (status) => {
                retryOp.reset();
                if (status.getCode() === 0) {
                  setClient(newClient);
                  setLoading(false);
                } else {
                  setClient(null);
                }
              },
              complete: () => {
                retryOp.retry(new Error('stream ended'));
              },
              error: (error) => {
                setClient(null);
                retryOp.retry(error);
              },
            });
          },
        );
      });
    }
    return () => {
      subscriptionPromise.then(() => {
        if (currentSubscription) {
          currentSubscription.unsubscribe();
        }
      });
      retryOp.stop();
    };
  }, [clusterID, passthroughEnabled, clusterStatus, cloudClient]);

  const context = React.useMemo(() => ({
    client,
    healthy,
    loading,
    clusterStatus,
  }), [client, healthy, loading, clusterStatus]);

  return (
    <VizierGRPCClientContext.Provider value={context}>
      {children}
    </VizierGRPCClientContext.Provider>
  );
};

export default VizierGRPCClientContext;
