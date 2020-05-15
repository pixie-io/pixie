import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { Spinner } from '../src/components/spinner/spinner';

storiesOf('Spinner', module)
  .add('Basic', () => (<Spinner />), {
    info: { inline: true },
    notes: 'Spinner component that spins, for use in loading states of components.',
  })
  .add('Dark', () => (
    <div style={{ backgroundColor: 'white' }}>
      <Spinner variant='dark' />
    </div>
  ), {
    info: { inline: true },
    notes: 'Dark variant for the spinner to use in light backgrounds.',
  });
