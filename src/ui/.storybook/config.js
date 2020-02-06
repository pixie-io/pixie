import {addDecorator, configure} from '@storybook/react';
import {withInfo} from '@storybook/addon-info';
import {withNotes} from '@storybook/addon-notes';

import './styles.scss';

addDecorator(withInfo);
addDecorator(withNotes);

const req = require.context('../stories', true, /.tsx$/);

function loadStories() {
  req.keys().forEach(req);
}

configure(loadStories, module);
