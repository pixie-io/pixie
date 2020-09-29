import React from 'react';

export interface SidebarContextProps {
  shortcutHelpInProfileMenu: boolean;
}

export const SidebarContext = React.createContext<SidebarContextProps>({
  shortcutHelpInProfileMenu: true,
});
