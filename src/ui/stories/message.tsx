import * as React from 'react';

import { MessageBox } from 'components/auth/message';
import { FrameElement } from './frame-utils';

export default {
  title: 'Auth/Message Box',
  component: MessageBox,
  decorators: [(Story) => <FrameElement width={500}><Story /></FrameElement>],
};

export const Completed = () => (
  <MessageBox
    title='Auth Completed'
    message='Please close this window and return to the CLI.'
  />
);

export const Error = () => (
  <MessageBox
    error
    title='Auth Failed'
    message='Login to this org is not allowed.'
  />
);

export const ErrorDetails = () => (
  <MessageBox
    error
    errorDetails='Internal error: bad things happened'
    title='Auth Failed'
    message='Login to this org is not allowed.'
  />
);

export const Code = () => (
  <MessageBox
    title='Code Box'
    message='Please copy and paste this code!'
    code='a9123sd12321asda-sd123213as-as12'
  />
);
