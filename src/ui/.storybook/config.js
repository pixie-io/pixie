import { addDecorator, configure } from '@storybook/react';
import { withInfo } from '@storybook/addon-info';
import { withNotes } from '@storybook/addon-notes';
import * as React from 'react';

import withTheme from './mui-theme-decorator';


addDecorator(withInfo);
addDecorator(withNotes);

// Provide a material-ui theme.
addDecorator(withTheme);

const req = require.context('../stories', true, /.tsx$/);

function loadStories() {
  req.keys().forEach(req);
}

configure(loadStories, module);
