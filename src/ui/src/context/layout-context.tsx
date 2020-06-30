import * as storage from 'common/storage';
import * as React from 'react';

import { useTheme } from '@material-ui/core/styles';
import useMediaQuery from '@material-ui/core/useMediaQuery';

type SetSplitSizesFunc = (sizes: [number, number]) => void;
type SetOpenFunc = React.Dispatch<React.SetStateAction<boolean>>;
type Splits = [number, number];

interface LayoutContextProps {
  editorSplitsSizes: [number, number];
  editorPanelOpen: boolean;
  setEditorSplitSizes: SetSplitSizesFunc;
  setEditorPanelOpen: SetOpenFunc;

  dataDrawerSplitsSizes: [number, number];
  dataDrawerOpen: boolean;
  setDataDrawerSplitsSizes: SetSplitSizesFunc;
  setDataDrawerOpen: SetOpenFunc;

  isMobile: boolean;
}

export const LayoutContext = React.createContext<LayoutContextProps>(null);

export const LayoutContextProvider = (props) => {
  const [editorSplitsSizes, setEditorSplitSizes] = storage.useLocalStorage<Splits>(
    storage.LIVE_VIEW_EDITOR_SPLITS_KEY, [40, 60],
  );
  const [editorPanelOpen, setEditorPanelOpen] = storage.useLocalStorage<boolean>(
    storage.LIVE_VIEW_EDITOR_OPENED_KEY, false,
  );

  const [dataDrawerSplitsSizes, setDataDrawerSplitsSizes] = storage.useLocalStorage<Splits>(
    storage.LIVE_VIEW_DATA_DRAWER_SPLITS_KEY,
    [60, 40],
  );
  const [dataDrawerOpen, setDataDrawerOpen] = storage.useLocalStorage<boolean>(
    storage.LIVE_VIEW_DATA_DRAWER_OPENED_KEY,
    false,
  );

  const theme = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down('xs')); // width < 600px
  React.useEffect(() => {
    if (isMobile) {
      setEditorPanelOpen(false);
    }
  }, [isMobile]);

  return (
    <LayoutContext.Provider
      value={{
        editorSplitsSizes,
        editorPanelOpen,
        setEditorSplitSizes,
        setEditorPanelOpen,
        dataDrawerSplitsSizes,
        dataDrawerOpen,
        setDataDrawerSplitsSizes,
        setDataDrawerOpen,
        isMobile,
      }}
    >
      {props.children}
    </LayoutContext.Provider>
  );
};
