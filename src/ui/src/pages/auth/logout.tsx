import * as React from 'react';
import Axios from 'axios';
import * as RedirectUtils from 'utils/redirect-utils';
import { BasePage } from './base';

// eslint-disable-next-line react/prefer-stateless-function
export const LogoutPage = () => {
  // eslint-disable-next-line class-methods-use-this
  React.useEffect(() => {
    Axios.post('/api/auth/logout').then(() => {
      localStorage.clear();
      sessionStorage.clear();
      RedirectUtils.redirect('', {});
    });
  }, []);

  return (
    <BasePage>
      Logging out...
    </BasePage>
  );
};
