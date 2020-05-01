import * as ls from 'common/localstorage';
import * as React from 'react';

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
}

export const LayoutContext = React.createContext<LayoutContextProps>(null);

export const LayoutContextProvider = (props) => {
  const [editorSplitsSizes, setEditorSplitSizes] =
    ls.useLocalStorage<Splits>(ls.LIVE_VIEW_EDITOR_SPLITS_KEY, [40, 60]);
  const [editorPanelOpen, setEditorPanelOpen] = ls.useLocalStorage<boolean>(ls.LIVE_VIEW_EDITOR_OPENED_KEY, false);

  const [dataDrawerSplitsSizes, setDataDrawerSplitsSizes] =
    ls.useLocalStorage<Splits>(ls.LIVE_VIEW_DATA_DRAWER_SPLITS_KEY, [60, 40]);
  const [dataDrawerOpen, setDataDrawerOpen] =
    ls.useLocalStorage<boolean>(ls.LIVE_VIEW_DATA_DRAWER_OPENED_KEY, false);

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
      }}>
      {props.children}
    </LayoutContext.Provider >
  );
};
