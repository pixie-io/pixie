import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { Spinner } from '../src/components/spinner/spinner';

storiesOf('Spinner', module)
  .add('Circular', () => (<Spinner />), {
    info: { inline: true },
    notes: 'Spinner component that spins, for use in loading states of components.',
  });
