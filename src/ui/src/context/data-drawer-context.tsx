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

import { WithChildren } from 'app/utils/react-boilerplate';

import { LayoutContext } from './layout-context';

interface DataDrawerContextProps {
  activeTab: string;
  setActiveTab: React.Dispatch<React.SetStateAction<string>>;
  openDrawerTab: (tab: string) => void;
}

export const DataDrawerContext = React.createContext<DataDrawerContextProps>(null);
DataDrawerContext.displayName = 'DataDrawerContext';

export const DataDrawerContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const [activeTab, setActiveTab] = React.useState<string>('');
  const { setDataDrawerOpen } = React.useContext(LayoutContext);
  const openDrawerTab = React.useCallback((tab: string) => {
    setDataDrawerOpen(true);
    setActiveTab(tab);
  }, [setDataDrawerOpen, setActiveTab]);

  return (
    <DataDrawerContext.Provider value={React.useMemo(() => ({
      activeTab,
      setActiveTab,
      openDrawerTab,
    }), [
      activeTab,
      setActiveTab,
      openDrawerTab,
    ])}>
      {children}
    </DataDrawerContext.Provider>
  );
});
DataDrawerContextProvider.displayName = 'DataDrawerContextProvider';
