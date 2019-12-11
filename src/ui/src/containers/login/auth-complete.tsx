import {DialogBox} from 'components/dialog-box/dialog-box';
import * as QueryString from 'query-string';
import * as React from 'react';

export const AuthComplete = ({ location }) => {
  const { err, siteName } = QueryString.parse(location.search);
  return (<DialogBox>
    <p className='pixie-auth-complete-msg'>
      {!err ? 'Authentication successful. Please close this page.' :
        err === 'token' ? [
          'Authentication failed. Please make sure the account has',
          <br />,
          'permission to use the site ',
          <strong>{siteName}</strong>,
        ] :
          'Authentication failed. Please try again later.'
      }
    </p>
  </DialogBox>);
};
