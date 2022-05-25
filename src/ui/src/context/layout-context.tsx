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

import { useMediaQuery } from '@mui/material';
import { useTheme } from '@mui/material/styles';

import { WithChildren } from 'app/utils/react-boilerplate';

type SetOpenFunc = React.Dispatch<React.SetStateAction<boolean>>;
type Splits = [number, number];
type SetSplitSizesFunc = (sizes: Splits) => void;

export interface LayoutContextProps {
  editorSplitsSizes: Splits;
  editorPanelOpen: boolean;
  setEditorSplitSizes: SetSplitSizesFunc;
  setEditorPanelOpen: SetOpenFunc;

  dataDrawerSplitsSizes: Splits;
  dataDrawerOpen: boolean;
  setDataDrawerSplitsSizes: SetSplitSizesFunc;
  setDataDrawerOpen: SetOpenFunc;

  isMobile: boolean;
}

export const LayoutContext = React.createContext<LayoutContextProps>(null);
LayoutContext.displayName = 'LayoutContext';

export const LayoutContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const [editorSplitsSizes, setEditorSplitSizes] = React.useState<Splits>([40, 60]);
  const [editorPanelOpen, setEditorPanelOpen] = React.useState<boolean>(false);

  const [dataDrawerSplitsSizes, setDataDrawerSplitsSizes] = React.useState<Splits>([60, 40]);
  const [dataDrawerOpen, setDataDrawerOpen] = React.useState<boolean>(false);

  const theme = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down('xs')); // width < 600px
  React.useEffect(() => {
    if (isMobile) {
      setEditorPanelOpen(false);
    }
  }, [isMobile, setEditorPanelOpen]);

  return (
    <LayoutContext.Provider
      value={React.useMemo(() => ({
        editorSplitsSizes,
        editorPanelOpen,
        setEditorSplitSizes,
        setEditorPanelOpen,
        dataDrawerSplitsSizes,
        dataDrawerOpen,
        setDataDrawerSplitsSizes,
        setDataDrawerOpen,
        isMobile,
      }), [
        editorSplitsSizes,
        editorPanelOpen,
        setEditorSplitSizes,
        setEditorPanelOpen,
        dataDrawerSplitsSizes,
        dataDrawerOpen,
        setDataDrawerSplitsSizes,
        setDataDrawerOpen,
        isMobile,
      ])}
    >
      {children}
    </LayoutContext.Provider>
  );
});
LayoutContextProvider.displayName = 'LayoutContextProvider';
