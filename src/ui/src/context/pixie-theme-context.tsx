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
import { createTheme, Theme, ThemeProvider } from '@mui/material/styles';
import { deepmerge } from '@mui/utils';

import {
  addSyntaxToPalette,
  DARK_BASE,
  DARK_THEME,
  LIGHT_THEME,
} from 'app/components/mui-theme';

export interface PixieThemeContextProps {
  theme: Theme;
  customTheme: Theme | null;
  parseAndSetTheme(raw: string): void;
  setThemeFromName(name: string): void;
}

export const PixieThemeContext = React.createContext<PixieThemeContextProps>({
  theme: DARK_THEME,
  customTheme: null,
  parseAndSetTheme: () => {},
  setThemeFromName: () => {},
});
PixieThemeContext.displayName = 'PixieThemeContext';

export const PixieThemeContextProvider = React.memo(({ children }) => {
  const [theme, setTheme] = React.useState<Theme>(DARK_THEME);
  const [parsedCustomTheme, setParsedCustomTheme] = React.useState<Theme | null>(null);

  const setThemeFromName = React.useCallback((name: string) => {
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
  }, [parsedCustomTheme]);

  const [prevRaw, setPrevRaw] = React.useState('');
  const parseAndSetTheme = React.useCallback((raw: string) => {
    if (raw === prevRaw) return;
    setPrevRaw(raw);

    // Try to parse theme and apply.
    try {
      const parsedTheme = JSON.parse(raw);
      // Only use the `palette` field from the theme, as we know these
      // values are safe to apply. Base atop the dark theme.
      const newTheme = createTheme({
        ...DARK_BASE,
        ...{
          palette: addSyntaxToPalette(deepmerge(DARK_THEME.palette, parsedTheme.palette, { clone: true })),
          shadows: deepmerge(DARK_THEME.shadows, parsedTheme.shadows, { clone: true }),
        },
      });
      setParsedCustomTheme(newTheme);
      setTheme(newTheme);
    } catch (e) {
      // eslint-disable-next-line no-console
      console.error('Failed to parse MUI theme');
    }
  }, [prevRaw]);

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
