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
import { GQLVizierConfig, GQLClusterStatus } from 'app/types/schema';
import { ClusterConfig } from 'app/api';
import { isDev } from 'app/utils/env';

export interface ClusterContextProps {
  selectedClusterID: string;
  selectedClusterName: string;
  selectedClusterPrettyName: string;
  selectedClusterUID: string;
  selectedClusterVizierConfig: GQLVizierConfig;
  selectedClusterStatus: GQLClusterStatus;
  setClusterByName: (id: string) => void;
}

export const ClusterContext = React.createContext<ClusterContextProps>(null);

export function useClusterConfig(): ClusterConfig | null {
  const { selectedClusterID, selectedClusterVizierConfig } = React.useContext(ClusterContext);
  return React.useMemo(() => {
    if (!selectedClusterID) return null;
    // If cloud is running in dev mode, automatically direct to Envoy's port, since there is
    // no GCLB to redirect for us in dev.
    const passthroughClusterAddress = selectedClusterVizierConfig.passthroughEnabled
      ? window.location.origin + (isDev() ? ':4444' : '') : undefined;
    return {
      id: selectedClusterID,
      attachCredentials: true,
      passthroughClusterAddress,
    };
  }, [selectedClusterID, selectedClusterVizierConfig]);
}
