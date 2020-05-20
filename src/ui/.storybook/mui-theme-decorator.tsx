import { DARK_THEME } from 'common/mui-theme';
import * as React from 'react';

import { ThemeProvider } from '@material-ui/core/styles';

export default (StoryFn) => (<ThemeProvider theme={DARK_THEME}><StoryFn /></ThemeProvider>);
