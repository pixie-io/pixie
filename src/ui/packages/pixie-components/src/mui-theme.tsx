import { createMuiTheme, Theme } from '@material-ui/core/styles';

declare module '@material-ui/core/styles/createPalette' {
  interface TypeBackground {
    one?: string;
    two?: string;
    three?: string;
    four?: string;
    five?: string;
    six?: string;
  }

  interface Palette {
    foreground: {
      one?: string;
      two?: string;
      three?: string;
      grey1?: string;
      grey2?: string;
      grey3?: string;
      grey4?: string;
      grey5?: string;
      white: string;
    };
    sideBar: {
      color?: string;
      colorShadow?: string;
      colorShadowOpacity?: number;
    };
    graph: {
      category?: string[];
      diverging?: string[];
      heatmap?: string[];
      primary?: string;
    };
    border: {
      focused?: string;
      unFocused?: string;
    };
  }

  interface PaletteOptions {
    foreground: {
      one?: string;
      two?: string;
      three?: string;
      four?: string;
      grey1?: string;
      grey2?: string;
      grey3?: string;
      grey4?: string;
      grey5?: string;
      white?: string;
    };
    sideBar: {
      color?: string;
      colorShadow?: string;
      colorShadowOpacity?: number;
    };
    graph: {
      category?: string[];
      diverging?: string[];
      heatmap?: string[];
      primary?: string;
    };
    border: {
      focused?: string;
      unFocused?: string;
    };
  }
}

declare module '@material-ui/core/styles/shape' {
  interface RoundedBorderRadius {
    small: string;
    large: string;
  }

  export interface Shape {
    leftRoundedBorderRadius: RoundedBorderRadius;
    rightRoundedBorderRadius: RoundedBorderRadius;
  }
}

export const scrollbarStyles = (theme: Theme) => {
  const commonStyle = (color) => ({
    borderRadius: theme.spacing(1.0),
    border: [['solid', theme.spacing(0.7), 'transparent']],
    backgroundColor: 'transparent',
    boxShadow: [['inset', 0, 0, theme.spacing(0.5), theme.spacing(1), color]],
  });
  return {
    '& ::-webkit-scrollbar': {
      width: theme.spacing(2),
      height: theme.spacing(2),
    },
    '& ::-webkit-scrollbar-track': commonStyle(theme.palette.background.one),
    '& ::-webkit-scrollbar-thumb': commonStyle(theme.palette.foreground.grey2),
    '& ::-webkit-scrollbar-corner': {
      backgroundColor: 'transparent',
    },
  };
};

export const DARK_THEME = createMuiTheme({
  shape: {
    leftRoundedBorderRadius: {
      large: '10px 0px 0px 10px',
      small: '5px 0px 0px 5px',
    },
    rightRoundedBorderRadius: {
      large: '0px 10px 10px 0px',
      small: '0px 5px 5px 0px',
    },
  },
  typography: {
    // The below rem conversions are based on a root font-size of 16px.
    h3: {
      fontSize: '1.125rem', // 18px
      fontWeight: 500,
      marginBottom: '16px',
      marginTop: '12px',
    },
    h2: {
      fontSize: '1.5rem', // 24px
      fontWeight: 500,
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
    h1: {
      fontSize: '2.125rem', // 34px
      fontWeight: 400,
    },
  },
  palette: {
    type: 'dark',
    border: {
      focused: '1px solid rgba(255, 255, 255, 0.2)',
      unFocused: '1px solid rgba(255, 255, 255, 0.1)',
    },
    sideBar: {
      color: '#161616',
      colorShadow: '#000000',
      colorShadowOpacity: 0.5,
    },
    common: {
      black: '#000000',
      white: '#ffffff',
    },
    primary: {
      main: '#12d6d6',
      dark: '#17aaaa',
      light: '#3ef3f3',
    },
    secondary: {
      main: '#24b2ff',
      dark: '#21a1e7',
      light: '#79d0ff',
    },
    success: {
      main: '#00dba6',
      dark: '#00bd8f',
      light: '#4dffd4',
    },
    warning: {
      main: '#f6a609',
      dark: '#dc9406',
      light: '#ffc656',
    },
    info: {
      main: '#f0de3d',
      dark: '#dac92f',
      light: '#fff48f',
    },
    error: {
      main: '#ff5e6d',
      dark: '#e54e5c',
      light: '#ff9fa8',
    },
    foreground: {
      one: '#b2b5bb',
      two: '#f2f2f2',
      three: '#9696a5',
      grey1: '#4a4c4f',
      grey2: '#353738',
      grey3: '#212324',
      grey4: '#596274',
      grey5: '#dbdde0',
      white: '#ffffff',
    },
    background: {
      default: '#161616',
      paper: '#292929',
      one: '#121212',
      two: '#212324',
      three: '#353535',
      four: '#161616',
      five: '#090909',
      six: '#242424',
    },
    divider: '#272822',
    text: {
      primary: '#e2e5ee', // foreground 1
      secondary: '#ffffff', // foreground 2
      disabled: '#',
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
      heatmap: [
        '#d6e8ed', // light-1
        '#cee0e5', // light-2
        '#91bfd9', // main
        '#549cc6', // dark-1
        '#1d78b5', // dark-2
      ],
      primary: '#39A8F5',
    },
  },
  overrides: {
    MuiDivider: {
      root: {
        backgroundColor: '#353535', // background three.
      },
    },
  },
});
