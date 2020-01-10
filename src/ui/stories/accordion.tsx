import {Accordion} from 'components/accordion';
import * as React from 'react';

import {storiesOf} from '@storybook/react';

storiesOf('Accordion', module)
  .add('Basic', () => (
    <Accordion
      items={[
        {
          title: 'Toggle1',
          key: 'toggle1',
          content: <div>content of menu 1</div>,
        },
        {
          title: 'Toggle2',
          key: 'toggle2',
          content: <div>contents of menu 2</div>,
        },
      ]}
    />
  ), {
    info: { inline: true },
    note: 'Accordion component, only one of the menu item is open at a time.',
  });
