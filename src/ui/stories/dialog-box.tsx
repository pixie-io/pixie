import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { DialogBox } from '../src/components/dialog-box/dialog-box';

storiesOf('DialogBox', module)
  .add('Basic', () => (
    <DialogBox
      width={480}
    >
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </DialogBox>
  ), {
    info: { inline: true },
    notes: 'This is a dialog box.',
  });
