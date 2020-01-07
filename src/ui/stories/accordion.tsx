import {Accordion} from 'components/accordion';
import * as React from 'react';

import {action} from '@storybook/addon-actions';
import {storiesOf} from '@storybook/react';

storiesOf('Accordion', module)
  .add('Basic', () => (
    <Accordion
      items={[
        {
          name: 'Toggle1',
          key: 'toggle1',
          children: [
            {
              name: 'content1',
              onClick: action('clicked'),
            },
            {
              name: 'content2',
              onClick: action('clicked'),
            },
          ],
        },
        {
          name: 'Toggle2',
          key: 'toggle2',
          children: [
            {
              name: 'content1',
              onClick: action('clicked'),
            },
          ],
        },
      ]}
    />
  ), {
    info: { inline: true },
    note: 'Accordion component, only one of the menu item is open at a time.',
  });
