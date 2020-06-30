import Axios from 'axios';
import * as React from 'react';
import * as RedirectUtils from 'utils/redirect-utils';

export const LogoutHandler = () => {
  Axios({
    method: 'post',
    url: '/api/auth/logout',
  }).then(() => {
    RedirectUtils.redirect('', {});
  });
};

// eslint-disable-next-line react/prefer-stateless-function
export class Logout extends React.Component<{}, {}> {
  // eslint-disable-next-line class-methods-use-this
  render() {
    Axios({
      method: 'post',
      url: '/api/auth/logout',
    }).then(() => {
      RedirectUtils.redirect('', {});
    });

    return (
      <div className='login'>
        <div className='login-body'>
          Logging out...
        </div>
      </div>
    );
  }
}
export default Logout;
