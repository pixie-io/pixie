import {Hello} from 'pixie-components';
import * as React from 'react';

import {storiesOf} from '@storybook/react';

storiesOf('Pixie Hello Component', module)
  .add('Basic', () => <Hello />, {
    info: { inline: true },
    note: 'Example of using pixie-components',
  });
