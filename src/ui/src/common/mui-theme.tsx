import { createMuiTheme, Theme } from '@material-ui/core/styles';

declare module '@material-ui/core/styles/createPalette' {
  interface TypeBackground {
    one?: string;
    two?: string;
    three?: string;
    four?: string;
    five?: string;
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
    topBar: {
      colorTop?: string;
      colorBottom?: string;
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
    topBar: {
      colorTop?: string;
      colorBottom?: string;
    };
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
  palette: {
    type: 'dark',
    topBar: {
      colorTop: '#212324',
      colorBottom: '#212324',
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
    },
    divider: '#272822',
    text: {
      primary: '#a6a8ae', // foreground 1
      secondary: '#ffffff', // foreground 2
      disabled: '#',
    },
    action: {
      active: '#a6a8ae', // foreground 1.
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
