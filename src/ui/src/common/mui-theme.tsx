import {createMuiTheme} from '@material-ui/core/styles';

declare module '@material-ui/core/styles/createPalette' {
  interface TypeBackground {
    one?: string;
    two?: string;
    three?: string;
    four?: string;
  }

  interface Palette {
    foreground: {
      one?: string;
      two?: string;
      grey1?: string;
      grey2?: string;
      grey3?: string;
    };
  }

  interface PaletteOptions {
    foreground: {
      one?: string;
      two?: string;
      grey1?: string;
      grey2?: string;
      grey3?: string;
    };
  }
}

export const DARK_THEME = createMuiTheme({
  palette: {
    type: 'dark',
    primary: {
      main: '#3fe7e7',
      dark: '#3cd6d6',
      light: '#89ffff',
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
    foreground: {
      one: '#a6abae',
      two: '#ffffff',
      grey1: '#4a4c4f',
      grey2: '#353738',
      grey3: '#212324',
    },
    background: {
      default: '#272822',
      paper: '#292929',
      one: '#121212',
      two: '#272822',
      three: '#353535',
      four: '#0C1714',
    },
    divider: '#272822',
    text: {
      primary: '#a6a8ae', // foreground 1
      secondary: '#ffffff', // foreground 2
      disabled: '#',
    },
    action: {
      active: '#a6a8ae', // foreground 1
    },
  },
});
