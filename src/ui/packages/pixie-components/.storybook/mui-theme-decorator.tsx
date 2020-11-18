import * as React from 'react';
import { DARK_THEME } from 'mui-theme';

import { ThemeProvider } from '@material-ui/core/styles';

export default (StoryFn) => (
  <ThemeProvider theme={DARK_THEME}>
    <StoryFn />
  </ThemeProvider>
);
