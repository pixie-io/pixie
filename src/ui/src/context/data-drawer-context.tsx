import * as React from 'react';

import { LayoutContext } from './layout-context';

interface DataDrawerContextProps {
  activeTab: string;
  setActiveTab: React.Dispatch<React.SetStateAction<string>>;
  openDrawerTab: (tab: string) => void;
}

export const DataDrawerContext = React.createContext<DataDrawerContextProps>(null);

export const DataDrawerContextProvider = (props) => {
  const [activeTab, setActiveTab] = React.useState<string>('');
  const { setDataDrawerOpen } = React.useContext(LayoutContext);
  const openDrawerTab = (tab: string) => {
    setDataDrawerOpen(true);
    setActiveTab(tab);
  };

  return (
    <DataDrawerContext.Provider value={{ activeTab, setActiveTab, openDrawerTab }}>
      {props.children}
    </DataDrawerContext.Provider>
  );
};
