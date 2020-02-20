import {createMuiTheme} from '@material-ui/core/styles';

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
    background: {
      default: '#292929',
      paper: '#353535',
    },
    divider: '#272822',
    text: {
      primary: '#a6a8ae',
      secondary: '#4a4c4f',
      disabled: '#212324',
    },
    action: {
      active: '#a6a8ae',
      hover: '#fff',
    },
  },
});
