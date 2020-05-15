import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { Alert } from '../src/components/alert/alert';

storiesOf('Alert', module)
  .add('Error Alert', () => (
    <Alert>
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </Alert>
  ), {
    info: { inline: true },
    notes: 'This is an error alert.',
  });
