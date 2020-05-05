import * as React from 'react';

import { LayoutContext } from './layout-context';

export type DataDrawerTabsKey = 'data' | 'errors' | 'stats';

interface DataDrawerContextProps {
  activeTab: DataDrawerTabsKey;
  setActiveTab: React.Dispatch<React.SetStateAction<DataDrawerTabsKey>>;
  openDrawerTab: (tab: DataDrawerTabsKey) => void;
}

export const DataDrawerContext = React.createContext<DataDrawerContextProps>(null);

export const DataDrawerContextProvider = (props) => {
  const [activeTab, setActiveTab] = React.useState<DataDrawerTabsKey>('data');
  const { setDataDrawerOpen } = React.useContext(LayoutContext);
  const openDrawerTab = (tab: DataDrawerTabsKey) => {
    setDataDrawerOpen(true);
    setActiveTab(tab);
  };

  return (
    <DataDrawerContext.Provider value={{ activeTab, setActiveTab, openDrawerTab }}>
      {props.children}
    </DataDrawerContext.Provider>
  );
};
