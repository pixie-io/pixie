import * as React from 'react';

import { storiesOf } from '@storybook/react';
import ActionCard from '../src/components/action-card/action-card';

storiesOf('ActionCard', module)
  .add('Basic', () => (
    <ActionCard title='The Card Title'>
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </ActionCard>
  ), {
    info: { inline: true },
    notes: 'This is the basic action card.',
  });
