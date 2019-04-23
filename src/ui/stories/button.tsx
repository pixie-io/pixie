import { action } from '@storybook/addon-actions';
import { storiesOf } from '@storybook/react';

import * as React from 'react';
import { Button } from 'react-bootstrap';

storiesOf('Button', module)
  .add('Active', () => (
    <Button
      variant='primary'
      onClick={action('onClick')}
    >
      Click me!
    </Button>
  ), {
      info: { inline: true },
      notes: 'This is a regular button that will be used in our UI.',
    })
  .add('Disabled', () => (
    <Button
      size='lg'
      variant='success'
      disabled
    >
      Save
    </Button>
  ), {
    info: { inline: true },
  });
