import React from 'react';

export interface SidebarContextProps {
  inLiveView: boolean;
}

export const SidebarContext = React.createContext<SidebarContextProps>({
  inLiveView: true,
});
