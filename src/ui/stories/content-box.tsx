import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { ContentBox } from '../src/components/content-box/content-box';

storiesOf('ContentBox', module)
  .add('With all headers', () => (
    <ContentBox
      headerText='Header'
      subheaderText='Subheader'
      secondaryText='Secondary text'
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
      headerText='Header'
    >
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </ContentBox>
  ), {
    info: { inline: true },
    notes: 'This is a content box that contains the content in our UI.',
  }).add('Resizable', () => (
    <ContentBox
      headerText='Header'
      resizable
      initialHeight={150}
    >
      <div>
        This is some content. It can be a string or more JSX.
      </div>
    </ContentBox>
  ), {
    info: { inline: true },
    notes: 'This is a content box that is resizable.',
  });
