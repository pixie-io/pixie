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

import { useQuery, gql } from '@apollo/client';

import { ClusterConfig } from 'app/api';
import { useSnackbar } from 'app/components';
import { LiveRouteContext, push } from 'app/containers/App/live-routing';
import { GQLClusterInfo, GQLVizierConfig, GQLClusterStatus } from 'app/types/schema';
import { stableSerializeArgs } from 'app/utils/args-utils';
import { isDev } from 'app/utils/env';

export interface ClusterContextProps {
  loading: boolean;
  selectedClusterID: string;
  selectedClusterName: string;
  selectedClusterPrettyName: string;
  selectedClusterUID: string;
  selectedClusterVizierConfig: GQLVizierConfig;
  selectedClusterStatus: GQLClusterStatus;
  selectedClusterStatusMessage: string;
  setClusterByName: (id: string) => void;
}

export const ClusterContext = React.createContext<ClusterContextProps>(null);
ClusterContext.displayName = 'ClusterContext';

type SelectedClusterInfo = Pick<
GQLClusterInfo,
'id' | 'clusterName' | 'prettyClusterName' | 'clusterUID' | 'vizierConfig' | 'status' | 'statusMessage'
>;

const invalidCluster = (name: string): SelectedClusterInfo => ({
  id: '',
  clusterUID: '',
  status: GQLClusterStatus.CS_UNKNOWN,
  statusMessage: '',
  vizierConfig: null,
  clusterName: name,
  prettyClusterName: name,
});

export const ClusterContextProvider = React.memo(({ children }) => {
  const showSnackbar = useSnackbar();

  const {
    scriptId, clusterName, args, embedState,
  } = React.useContext(LiveRouteContext);

  const { data, loading, error } = useQuery<{
    clusterByName: SelectedClusterInfo
  }>(
    gql`
      query selectedClusterInfo($name: String!) {
        clusterByName(name: $name) {
          id
          clusterName
          prettyClusterName
          clusterUID
          vizierConfig {
              passthroughEnabled
          }
          status
          statusMessage
        }
      }
    `,
    { pollInterval: 60000, fetchPolicy: 'cache-and-network', variables: { name: clusterName } },
  );

  const cluster = data?.clusterByName ?? invalidCluster(clusterName);

  const serializedArgs = stableSerializeArgs(args);
  const setClusterByName = React.useCallback((name: string) => {
    push(name, scriptId, args, embedState);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [push, scriptId, serializedArgs, embedState]);

  const clusterContext = React.useMemo(() => ({
    selectedClusterID: cluster?.id,
    selectedClusterName: cluster?.clusterName,
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterUID: cluster?.clusterUID,
    selectedClusterVizierConfig: cluster?.vizierConfig,
    selectedClusterStatus: cluster?.status,
    selectedClusterStatusMessage: cluster?.statusMessage,
    setClusterByName,
    loading,
  }), [
    cluster,
    setClusterByName,
    loading,
  ]);

  React.useEffect(() => {
    if (clusterName && error?.message) {
      // This is an error with pixie cloud, it is probably not relevant to the user.
      // Show a generic error message instead.
      showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
      // eslint-disable-next-line no-console
      console.error(error?.message);
    }
  }, [showSnackbar, clusterName, error?.message]);

  return (
    <ClusterContext.Provider value={clusterContext}>
      {children}
    </ClusterContext.Provider>
  );
});
ClusterContextProvider.displayName = 'ClusterContextProvider';

export function useClusterConfig(): ClusterConfig | null {
  const { loading, selectedClusterID, selectedClusterVizierConfig } = React.useContext(ClusterContext);
  return React.useMemo(() => {
    if (loading || !selectedClusterID) return null;
    // If cloud is running in dev mode, automatically direct to Envoy's port, since there is
    // no GCLB to redirect for us in dev.
    const passthroughClusterAddress = window.location.origin + (isDev() ? ':4444' : '');
    return {
      id: selectedClusterID,
      attachCredentials: true,
      passthroughClusterAddress,
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectedClusterID, selectedClusterVizierConfig, loading]);
}
