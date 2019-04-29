import { action } from '@storybook/addon-actions';
import { storiesOf } from '@storybook/react';

import * as React from 'react';
import { ContentBox } from '../src/components/content-box/content-box';

storiesOf('ContentBox', module)
  .add('With all headers', () => (
    <ContentBox
      headerText={'Header'}
      subheaderText={'Subheader'}
      secondaryText={'Secondary text'}
    >
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </ContentBox>
  ), {
      info: { inline: true },
      notes: 'This is a content box that contains the content in our UI.',
    }).add('With no extra headers', () => (
    <ContentBox
      headerText={'Header'}
    >
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </ContentBox>
  ), {
      info: { inline: true },
      notes: 'This is a content box that contains the content in our UI.',
    });
