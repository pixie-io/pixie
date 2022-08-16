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

import { CssBaseline } from '@mui/material';
import { Theme, ThemeProvider } from '@mui/material/styles';

import { isPixieEmbedded } from 'app/common/embed-context';
import {
  createPixieTheme,
  DARK_THEME,
  LIGHT_THEME,
} from 'app/components/mui-theme';
import { ThemeSelectionContext } from 'app/components/theme-selector/theme-selector';
import { WithChildren } from 'app/utils/react-boilerplate';

export interface PixieThemeContextProps {
  theme: Theme;
  customTheme: Theme | null;
  parseAndSetTheme(raw: string): void;
  setThemeFromName(name: string, override?: boolean): void;
}

export const PixieThemeContext = React.createContext<PixieThemeContextProps>({
  theme: DARK_THEME,
  customTheme: null,
  parseAndSetTheme: () => {},
  setThemeFromName: () => {},
});
PixieThemeContext.displayName = 'PixieThemeContext';

export const PixieThemeContextProvider = React.memo<WithChildren>(({ children }) => {
  const [theme, setTheme] = React.useState<Theme>(DARK_THEME);
  const [parsedCustomTheme, setParsedCustomTheme] = React.useState<Theme | null>(null);

  const setThemeFromName = React.useCallback((name: string, override = false) => {
    if (theme === parsedCustomTheme && !override) return;
    switch (name) {
      case 'light':
        setTheme(LIGHT_THEME);
        break;
      case 'custom':
        if (parsedCustomTheme) {
          setTheme(parsedCustomTheme);
        }
        break;
      default:
        setTheme(DARK_THEME);
        break;
    }
  }, [theme, parsedCustomTheme]);

  const [prevRaw, setPrevRaw] = React.useState('');
  const parseAndSetTheme = React.useCallback((raw: string) => {
    if (raw === prevRaw) return;
    setPrevRaw(raw);

    // Try to parse theme and apply.
    try {
      const parsedTheme = JSON.parse(raw);
      const newTheme = createPixieTheme(parsedTheme);
      setParsedCustomTheme(newTheme);
      setTheme(newTheme);
    } catch (e) {
      // eslint-disable-next-line no-console
      console.error('Failed to parse MUI theme', raw);
    }
  }, [prevRaw]);

  // Watch for changes to user preference - if they've told Pixie to match their system preference, we want to
  // honor that preference right away if it changes. We don't do this if the theme was overridden in an embed, though.
  const { preferenceLoaded, theme: userSelectedTheme } = React.useContext(ThemeSelectionContext);
  React.useEffect(() => {
    const isEmbedded = isPixieEmbedded();
    if (!isEmbedded && preferenceLoaded && !parsedCustomTheme) {
      setThemeFromName(userSelectedTheme.palette.mode);
    }
  }, [preferenceLoaded, userSelectedTheme, setThemeFromName, parsedCustomTheme]);

  const ctx = React.useMemo(() => ({
    theme,
    customTheme: parsedCustomTheme,
    parseAndSetTheme,
    setThemeFromName,
  }), [theme, parsedCustomTheme, parseAndSetTheme, setThemeFromName]);

  return (
    <PixieThemeContext.Provider value={ctx}>
      <ThemeProvider theme={theme}>
        <CssBaseline />
        {children}
      </ThemeProvider>
    </PixieThemeContext.Provider>
  );
});
PixieThemeContextProvider.displayName = 'PixieThemeContextProvider';
