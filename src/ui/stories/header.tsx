import { action } from '@storybook/addon-actions';
import { storiesOf } from '@storybook/react';

import * as React from 'react';
import { Header } from '../src/components/header/header';

storiesOf('Header', module)
  .add('Basic header', () => (
    <Header
      primaryHeading={'Header'}
      secondaryHeading={'secondary'}
    />
  ), {
      info: { inline: true },
      notes: 'This is a header that goes at the top of the page.',
    });
