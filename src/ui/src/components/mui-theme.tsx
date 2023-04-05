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


import { alpha, PaletteMode } from '@mui/material';
import { createTheme, Theme, type ThemeOptions as OriginalThemeOptions } from '@mui/material/styles';
import type {
  PaletteOptions as AugmentedPaletteOptions,
  SimplePaletteColorOptions,
} from '@mui/material/styles/createPalette';
import { Shadows } from '@mui/material/styles/shadows';
import { deepmerge } from '@mui/utils';

// These aren't quite the Material UI colors, but they follow the same principles.
const COLORS = {
  PRIMARY: {
    '100': '#D0FBFB',
    '200': '#A1F7F7',
    '300': '#6DF3F3',
    '400': '#35EEEE',
    '500': '#12D6D6', // Brand primary color!
    '600': '#0DA0A0',
    '700': '#096C6C',
    '800': '#053838',
    '900': '#031C1C',
  },
  SECONDARY: {
    '100': '#E0F4FF',
    '200': '#C2EAFF',
    '300': '#99DBFF',
    '400': '#61C8FF',
    '500': '#24B2FF',
    '600': '#0095E5',
    '700': '#006EA8',
    '800': '#004266',
    '900': '#002133',
  },
  NEUTRAL: {
    '100': '#FFFFFF',
    '200': '#E6E6EA',
    '300': '#C3C3CB',
    '400': '#686875',
    '500': '#4E4E4E',
    '600': '#3B3B3B',
    '700': '#333333',
    '800': '#232323',
    '850': '#1E1E1E',
    '900': '#121212',
    '1000': '#0A0A0A',
  },
  SUCCESS: {
    '300': '#50F6CE',
    '400': '#20F3C1',
    '500': '#0BD3A3',
    '600': '#0AC296',
  },
  ERROR: {
    '300': '#FFA8B0',
    '400': '#FF7582',
    '500': '#FF5E6D',
    '600': '#FF4C5D',
  },
  WARNING: {
    '300': '#FACA6B',
    '400': '#F8B83A',
    '500': '#F6A609',
    '600': '#E79C08',
  },
  INFO: {
    '300': '#B5E4FD',
    '400': '#83D2FB',
    '500': '#53C0FA',
    '600': '#20ADF9',
  },
  // Note: these two colors are similar enough that RGB interpolation doesn't create a noticeable grey zone.
  // If it did, we'd need to manually compute a few middle points in a colorspace like HCL or LAB to get a better one.
  GRADIENT: 'linear-gradient(to right, #00DBA6 0%, #24B2FF 100%)',
};

interface SyntaxPalette {
  /** Default color for tokens that don't match any other rules */
  normal: string;
  /** Scoping tokens, such as (parens), [brackets], and {braces} */
  scope: string;
  /** Tokens that separate others, such as =,.:; */
  divider: string;
  /** Tokens that have something wrong (semantic or syntax errors, etc) */
  error: string;
  // Primitives...
  boolean: string;
  number: string;
  string: string;
  nullish: string;
}

declare module '@mui/material/styles/createPalette' {
  interface TypeBackground {
    one: string;
    two: string;
    three: string;
    four: string;
    five: string;
    six: string;
  }

  interface Palette {
    foreground: {
      one: string;
      two: string;
      three: string;
      grey1: string;
      grey2: string;
      grey3: string;
      grey4: string;
      grey5: string;
      white: string;
    };
    sideBar: {
      color: string;
      colorShadow: string;
      colorShadowOpacity: number;
    };
    graph: {
      category: string[];
      diverging: string[];
      ramp: string[];
      heatmap: string[];
      primary: string;
      flamegraph: {
        kernel: string,
        java: string,
        app: string,
        k8s: string,
      };
    };
    border: {
      focused: string;
      unFocused: string;
    };
    syntax: SyntaxPalette;
  }

  interface PaletteOptions {
    foreground: {
      one: string;
      two: string;
      three: string;
      grey1: string;
      grey2: string;
      grey3: string;
      grey4: string;
      grey5: string;
    };
    sideBar: {
      color: string;
      colorShadow: string;
      colorShadowOpacity: number;
    };
    graph: {
      primary: string;
      category: string[];
      diverging: string[];
      ramp: string[];
      heatmap: string[];
      flamegraph: {
        kernel: string,
        java: string,
        app: string,
        k8s: string,
      };
    };
    border: {
      focused: string;
      unFocused: string;
    };
    syntax: SyntaxPalette;
  }
}

declare module '@mui/material/styles' {
  interface RoundedBorderRadius {
    small: string;
    large: string;
  }

  export interface Shape {
    borderRadius: string | number;
    leftRoundedBorderRadius: RoundedBorderRadius;
    rightRoundedBorderRadius: RoundedBorderRadius;
  }

  interface Theme {
    shape: Shape;
  }

  interface TypographyVariants {
    monospace: React.CSSProperties;
  }

  interface TypographyVariantOptions {
    monospace?: React.CSSProperties;
  }
}

declare module '@mui/material/Typography' {
  interface TypographyPropsVariantOverrides {
    monospace: true;
  }
}

declare module '@mui/material/styles/createTheme' {
  interface ThemeOptions {
    shape?: Partial<Theme['shape']>;
  }
}

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export const scrollbarStyles = (theme: Theme) => {
  const commonStyle = (color: string) => ({
    borderRadius: theme.spacing(1.0),
    border: [['solid', theme.spacing(0.7), 'transparent']],
    backgroundColor: 'transparent',
    boxShadow: [['inset', 0, 0, theme.spacing(0.5), theme.spacing(1), color]],
  });
  return {
    '& ::-webkit-scrollbar': {
      '-webkit-appearance': 'none',
      width: theme.spacing(2),
      height: theme.spacing(2),
    },
    '& ::-webkit-scrollbar-track': commonStyle(theme.palette.background.default),
    '& ::-webkit-scrollbar-thumb': commonStyle(theme.palette.foreground.grey2),
    '& ::-webkit-scrollbar-corner': {
      backgroundColor: 'transparent',
    },
  };
};

export const EDITOR_THEME_MAP: Record<PaletteMode, string> = {
  dark: 'vs-dark',
  light: 'vs-light',
};

// Note: not explicitly typed, as it's a messy nested partial of ThemeOptions.
// When combined with LIGHT_THEME and DARK_THEME below, however, TypeScript
// will still recognize if the merged object is missing anything.
export const COMMON_THEME = {
  shape: {
    borderRadius: 5,
    leftRoundedBorderRadius: {
      large: '10px 0px 0px 10px',
      small: '5px 0px 0px 5px',
    },
    rightRoundedBorderRadius: {
      large: '0px 10px 10px 0px',
      small: '0px 5px 5px 0px',
    },
  },
  zIndex: {
    drawer: 1075, // Normally higher than appBar and below modal. We want it below appBar.
  },
  typography: {
    // The below rem conversions are based on a root font-size of 16px.
    h1: {
      fontSize: '2.125rem', // 34px
      fontWeight: 400,
    },
    h2: {
      fontSize: '1.5rem', // 24px
      fontWeight: 500,
    },
    h3: {
      fontSize: '1.125rem', // 18px
      fontWeight: 500,
      marginBottom: '16px',
      marginTop: '12px',
    },
    h4: {
      fontSize: '0.875rem', // 14px
      fontWeight: 500,
    },
    caption: {
      fontSize: '0.875rem', // 14px
    },
    subtitle1: {
      fontSize: '1rem', // 16px
    },
    subtitle2: {
      fontSize: '0.875rem', // 14px
      fontWeight: 400,
    },
    monospace: {
      fontFamily: '"Roboto Mono", monospace',
    },
  },
  palette: {
    sideBar: {
      color: COLORS.NEUTRAL[900],
      colorShadow: '#000000',
      colorShadowOpacity: 0.5,
    },
    common: {
      black: '#000000',
      white: '#FFFFFF',
    },
    primary: {
      main: COLORS.PRIMARY[500],
      dark: COLORS.PRIMARY[600],
      light: COLORS.PRIMARY[400],
    },
    secondary: {
      main: COLORS.SECONDARY[500],
      dark: COLORS.SECONDARY[600],
      light: COLORS.SECONDARY[400],
    },
    action: {
      active: '#a6a8ae', // foreground 1.
    },
    graph: {
      category: [
        '#21a1e7', // one
        '#2ca02c', // two
        '#98df8a', // three
        '#aec7e8', // four
        '#ff7f0e', // five
        '#ffbb78', // six
      ],
      diverging: [
        '#cc0020', // left-dark
        '#e77866', // left-main
        '#f6e7e1', // left-light
        '#d6e8ed', // right-light
        '#91bfd9', // right-main
        '#1d78b5', // right-dark
      ],
      ramp: [
        '#fff48f', // info-light
        '#f0de3d', // info-main
        '#dac92f', // info-dark
        '#ffc656', // warning-light
        '#f6a609', // warning-main
        '#dc9406', // warning-dark
        '#ff5e6d', // error-main
        '#e54e5c', // error-dark
      ],
      heatmap: [
        '#d6e8ed', // light-1
        '#cee0e5', // light-2
        '#91bfd9', // main
        '#549cc6', // dark-1
        '#1d78b5', // dark-2
      ],
      primary: '#39a8f5',
    },
  },
  components: {
    MuiCssBaseline: {
      // Global styles.
      styleOverrides: {
        '#root': {
          height: '100%',
          width: '100%',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
        },
        html: {
          height: '100%',
        },
        body: {
          height: '100%',
          overflow: 'hidden',
          margin: 0,
          boxSizing: 'border-box',
        },
        ':focus': {
          outline: 'none !important',
        },
        a: {
          // color: 'primary.main', // Overridden
          textDecoration: 'none',
          '&:hover': {
            textDecoration: 'underline',
          },
        },
        // Vega-Tooltip (included in Vega-Lite) doesn't quite get right math when constraining a tooltip to the screen.
        // Also, we sometimes use titles so long they fill the screen (like generated method signatures in flamegraphs).
        // Sticky positioning and multi-line word wrap, respectively, handle both issues with just CSS.
        '#vg-tooltip-element.vg-tooltip': {
          maxWidth: 'min(400px, 80vw)',
          width: 'fit-content',
          position: 'sticky',
          // The div#root element confuses sticky positioning without this negative margin.
          marginTop: '-100vh',
          '& > h2': {
            display: '-webkit-box',
            maxWidth: '100%',
            WebkitLineClamp: 4,
            WebkitBoxOrient: 'vertical',
            overflow: 'hidden',
            overflowWrap: 'anywhere',
          },
        },
      },
    },
    MuiMenuItem: {
      styleOverrides: {
        root: {
          width: '100%',
        },
      },
    },
  },
};

type DeepPartial<T> = T extends object ? {
  [P in keyof T]?: DeepPartial<T[P]>;
} : T;

function augmentPalette(base: DeepPartial<AugmentedPaletteOptions>): AugmentedPaletteOptions {
  const merged: Omit<AugmentedPaletteOptions, 'syntax'> = deepmerge<any>(
    COMMON_THEME.palette,
    base,
    { clone: true },
  );
  return {
    ...merged,
    syntax: {
      normal: merged.text.secondary,
      scope: merged.text.secondary,
      divider: merged.foreground.three,
      error: (merged.error as Required<SimplePaletteColorOptions>).main,
      boolean: (merged.success as Required<SimplePaletteColorOptions>).main,
      number: (merged.info as Required<SimplePaletteColorOptions>).main,
      string: merged.mode === 'dark' ? merged.graph.ramp[0] : merged.graph.ramp[2],
      nullish: merged.foreground.three,
    },
  };
}

function augmentComponents(palette: DeepPartial<AugmentedPaletteOptions>) {
  return deepmerge(
    COMMON_THEME.components,
    {
      MuiCssBaseline: {
        styleOverrides: {
          a: {
            color: (palette.primary as SimplePaletteColorOptions)?.main ?? COMMON_THEME.palette?.primary.main,
          },
        },
      },
    },
    { clone: true },
  );
}

export const DARK_BASE = {
  palette: {
    mode: 'dark' as const,
    divider: COLORS.NEUTRAL[800],
    foreground: {
      one: COLORS.NEUTRAL[300],
      two: COLORS.NEUTRAL[200],
      three: COLORS.NEUTRAL[400],
      grey1: COLORS.NEUTRAL[500],
      grey2: COLORS.NEUTRAL[700],
      grey3: COLORS.NEUTRAL[800],
      grey4: COLORS.NEUTRAL[400],
      grey5: COLORS.NEUTRAL[200],
    },
    background: {
      default: COLORS.NEUTRAL[900],
      paper: COLORS.NEUTRAL[900],
      one: COLORS.NEUTRAL[1000],
      two: COLORS.NEUTRAL[900],
      three: COLORS.NEUTRAL[850],
      four: COLORS.NEUTRAL[800],
      five: COLORS.NEUTRAL[700],
      six: COLORS.NEUTRAL[600],
    },
    text: {
      primary: COLORS.NEUTRAL[200],
      secondary: COLORS.NEUTRAL[100],
      disabled: alpha(COLORS.NEUTRAL[200], 0.7),
    },
    border: {
      focused: `1px solid ${alpha(COLORS.NEUTRAL[100], 0.2)}`,
      unFocused: `1px solid ${alpha(COLORS.NEUTRAL[100], 0.1)}`,
    },
    success: {
      main: COLORS.SUCCESS[500],
      dark: COLORS.SUCCESS[600],
      light: COLORS.SUCCESS[400],
    },
    warning: {
      main: COLORS.WARNING[500],
      dark: COLORS.WARNING[600],
      light: COLORS.WARNING[400],
    },
    info: {
      main: COLORS.INFO[500],
      dark: COLORS.INFO[600],
      light: COLORS.INFO[400],
    },
    error: {
      main: COLORS.ERROR[500],
      dark: COLORS.ERROR[600],
      light: COLORS.ERROR[400],
    },
    graph: {
      flamegraph: {
        kernel: '#98df8a',
        java: '#31d0f3',
        app: '#31d0f3',
        k8s: '#4796c1',
      },
    },
  },
  components: {
    MuiDivider: {
      styleOverrides: {
        root: {
          backgroundColor: COLORS.NEUTRAL[700],
        },
      },
    },
  },
};

export const LIGHT_BASE = {
  palette: {
    mode: 'light' as const,
    divider: '#dbdde0',
    foreground: {
      one: '#4f4f4f',
      two: '#000000',
      three: '#a9adb1',
      grey1: '#cacccf',
      grey2: '#dbdde0',
      grey3: '#f6f6f6',
      grey4: '#a9adb1',
      grey5: '#000000',
    },
    background: {
      default: '#f6f6f6',
      paper: '#fbfbfb',
      one: '#ffffff',
      two: '#fbfbfb',
      three: '#fbfcfd',
      four: '#ffffff',
      five: '#f5f5f5',
      six: '#f8f9fa',
    },
    text: {
      primary: 'rgba(0, 0, 0, 0.87)', // Material default
      secondary: '#000000',
      disabled: 'rgba(0, 0, 0, 0.7)', // Muted
    },
    border: {
      focused: '1px solid rgba(0, 0, 0, 0.2)',
      unFocused: '1px solid rgba(0, 0, 0, 0.1)',
    },
    success: {
      main: COLORS.SUCCESS[500],
      dark: COLORS.SUCCESS[600],
      light: COLORS.SUCCESS[400],
    },
    warning: {
      main: COLORS.WARNING[500],
      dark: COLORS.WARNING[600],
      light: COLORS.WARNING[400],
    },
    info: {
      main: COLORS.INFO[500],
      dark: COLORS.INFO[600],
      light: COLORS.INFO[400],
    },
    error: {
      main: COLORS.ERROR[500],
      dark: COLORS.ERROR[600],
      light: COLORS.ERROR[400],
    },
    graph: {
      flamegraph: {
        kernel: '#90cb84',
        java: '#00b1d8',
        app: '#00b1d8',
        k8s: '#4796c1',
      },
    },
  },
  components: {
    MuiDivider: {
      styleOverrides: {
        root: {
          backgroundColor: '#a9adb1', // foreground three.
        },
      },
    },
  },
};

export function createPixieTheme(options: DeepPartial<OriginalThemeOptions>): Theme {
  const base = options?.palette?.mode === 'light' ? LIGHT_BASE : DARK_BASE;
  const merged: OriginalThemeOptions = deepmerge<any>(COMMON_THEME, base, { clone: true });
  // When a custom theme override is given, we only copy color and shadow information (as we know those are safe)
  merged.palette = augmentPalette(deepmerge(merged.palette, options.palette ?? {}, { clone: true }));
  merged.components = augmentComponents(merged.palette);
  if (Array.isArray(options.shadows) && options.shadows.length) merged.shadows = options.shadows as Shadows;
  return createTheme(merged);
}

export const DARK_THEME = createPixieTheme({ palette: { mode: 'dark' } });
export const LIGHT_THEME = createPixieTheme({ palette: { mode: 'light' } });
