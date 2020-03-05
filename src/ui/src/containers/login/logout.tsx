import Axios from 'axios';
import * as React from 'react';
import * as RedirectUtils from 'utils/redirect-utils';

export class Logout extends React.Component<{}, {}> {
  render() {
    Axios({
      method: 'post',
      url: '/api/auth/logout',
    }).then((response) => {
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
