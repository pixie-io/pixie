import * as React from 'react';

import { storiesOf } from '@storybook/react';
import { AuthBox } from 'components/auth/auth-box';
import { SignupMarcom } from 'components/auth/signup-marcom';
import { Footer } from 'components/auth/footer';
import { MessageBox } from 'components/auth/message';
import { FrameElement } from './frame-utils';

storiesOf('Auth | AuthBox', module)
  .add('Login', () => (
    <FrameElement width={500}>
      <AuthBox variant='login' />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'The login box',
  })
  .add('Signup', () => (
    <FrameElement width={500}>
      <AuthBox variant='signup' />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'The signup box',
  });

storiesOf('Auth | Signup Marcom', module)
  .add('Signup Marcom message', () => (
    <FrameElement width={500}>
      <SignupMarcom />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'The signup marcom message',
  });

storiesOf('Auth | Message Box', module)
  .add('Message box', () => (
    <FrameElement width={500}>
      <MessageBox
        title='Auth Completed'
        message='Please close this window and return to the CLI.'
      />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'Message boxes used on the auth pages',
  })
  .add('Message box with error', () => (
    <FrameElement width={500}>
      <MessageBox
        error
        title='Auth Failed'
        message='Login to this org is not allowed.'
      />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'Message box with error',
  })
  .add('Message box with error & details', () => (
    <FrameElement width={500}>
      <MessageBox
        error
        errorDetails='Internal error: bad things happened'
        title='Auth Failed'
        message='Login to this org is not allowed.'
      />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'Message box with error',
  });

storiesOf('Auth | Footer', module)
  .add('Basic', () => (
    <FrameElement width={500}>
      <Footer />
    </FrameElement>
  ), {
    info: { inline: false },
    notes: 'The footer on all auth pages',
  });
