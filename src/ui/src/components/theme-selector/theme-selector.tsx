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

import { ToggleButton, ToggleButtonGroup } from '@mui/material';
import { Theme } from '@mui/material/styles';

import { DARK_THEME, LIGHT_THEME } from 'app/components/mui-theme';
import { PixieThemeContext } from 'app/context/pixie-theme-context';
import { WithChildren } from 'app/utils/react-boilerplate';

function useMediaQuery(query: string): boolean {
  const [matches, setMatches] = React.useState(false);
  React.useEffect(() => {
    const matcher = window.matchMedia(query);
    setMatches(matcher.matches);
    const listener = ({ matches: eventMatches }) => {
      setMatches(eventMatches);
    };
    matcher.addEventListener('change', listener);
    return () => {
      matcher.removeEventListener('change', listener);
    };
  }, [query]);

  return matches;
}

export type PixieThemeMode = 'dark' | 'light' | 'system';
export const DEFAULT_THEME_MODE: PixieThemeMode = 'dark';

const THEME_LOCAL_STORAGE_KEY = 'preferredTheme';

export interface ThemeSelectionContextProps {
  preferenceLoaded: boolean;
  mode: PixieThemeMode;
  theme: Theme;
  setMode: (mode: PixieThemeMode) => void;
}
export const ThemeSelectionContext = React.createContext<ThemeSelectionContextProps>({
  preferenceLoaded: false,
  mode: 'dark',
  theme: DARK_THEME,
  setMode: () => {},
});

export const ThemeSelectionContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const [preferenceLoaded, setPreferenceLoaded] = React.useState(false);
  const [stored, setStored] = React.useState<PixieThemeMode>(DEFAULT_THEME_MODE);

  // Try once to read localStorage
  React.useEffect(() => {
    try {
      const storedReal = globalThis.localStorage?.getItem(THEME_LOCAL_STORAGE_KEY);
      if (storedReal && ['dark', 'light', 'system'].includes(storedReal)) {
        setStored(storedReal as unknown as typeof stored);
      } else {
        setStored(DEFAULT_THEME_MODE);
      }
      setPreferenceLoaded(true);
    } catch (_) {/* If we don't have access to localStorage, we've likely been embedded with the theme overridden. */}
  }, []);

  const [systemChoice, setSystemChoice] = React.useState<Theme>(null);
  // Per spec, (prefers-color-scheme: light) also matches if there's no preference set. So, we test for dark mode.
  const prefersDark = useMediaQuery('(prefers-color-scheme: dark)');
  React.useEffect(() => {
    if (prefersDark) setSystemChoice(DARK_THEME);
    else setSystemChoice(LIGHT_THEME);
  }, [prefersDark]);

  const [chosen, setChosen] = React.useState<Theme>(DARK_THEME);
  React.useEffect(() => {
    switch (stored) {
      case 'dark':
        setChosen(DARK_THEME);
        break;
      case 'light':
        setChosen(LIGHT_THEME);
        break;
      case 'system':
        setChosen(systemChoice);
        break;
    }
  }, [stored, systemChoice]);

  const setMode = React.useCallback((mode: PixieThemeMode) => {
    try {
      if (mode === DEFAULT_THEME_MODE) globalThis.localStorage?.removeItem(THEME_LOCAL_STORAGE_KEY);
      else globalThis.localStorage?.setItem(THEME_LOCAL_STORAGE_KEY, mode);
    } catch (_) {/* No big deal if we can't persist the setting. It just won't persist between refreshes. */}
    setStored(mode);
  }, []);

  const ctx = React.useMemo(() => ({
    preferenceLoaded,
    mode: stored,
    theme: chosen,
    setMode,
  }), [stored, chosen, setMode, preferenceLoaded]);

  return <ThemeSelectionContext.Provider value={ctx}>{children}</ThemeSelectionContext.Provider>;
});
ThemeSelectionContextProvider.displayName = 'ThemeSelectionContextProvider';

export const ThemeSelectorToggles = React.memo(() => {
  const { theme: actualTheme, customTheme } = React.useContext(PixieThemeContext);
  const { mode: selectedMode, setMode: setSelectedMode } = React.useContext(ThemeSelectionContext);

  const isCustom = customTheme && actualTheme === customTheme;

  const onChange = React.useCallback((_, newSelection) => {
    // PixieThemeContextProvider watches this value and updates the actual theme on its own.
    setSelectedMode(newSelection);
  }, [setSelectedMode]);

  return (
    <ToggleButtonGroup
      size='small'
      fullWidth
      value={selectedMode}
      disabled={isCustom}
      exclusive
      onChange={onChange}
      aria-label='Select Theme'
    >
      <ToggleButton value='dark'>Dark</ToggleButton>
      <ToggleButton value='system'>System</ToggleButton>
      <ToggleButton value='light'>Light</ToggleButton>
    </ToggleButtonGroup>
  );
});
ThemeSelectorToggles.displayName = 'ThemeSelectorToggles';
