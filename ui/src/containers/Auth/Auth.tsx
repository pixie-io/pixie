import Auth0Lock from 'auth0-lock';
import Axios from 'axios';
import * as React from 'react';

const AUTH0_DOMAIN = 'pixie-labs.auth0.com';
const AUTH0_CLIENT_ID = 'qaAfEHQT7mRt6W0gMd9mcQwNANz9kRup';

interface AuthProps {
  containerID?: string;
}

export class Auth extends React.Component<AuthProps, any>  {
  public static defaultProps: Partial<AuthProps> = {
    containerID: 'pl-auth0-lock-container',
  };

  private _lock: Auth0LockStatic;

  componentDidMount() {
    this._lock = new Auth0Lock(AUTH0_CLIENT_ID, AUTH0_DOMAIN, {
      auth: {
        redirectUrl: window.location.origin + '/login',
        responseType: 'token',
        params: {
          scope: 'openid profile user_metadata email',
        },
      },
      allowSignUp: true,
      container: this.props.containerID,
      initialScreen: 'login',
    });

    if (this.isAuthenticated()) {
      this._lock.hide();
      return;
    }

    this._lock.show();
    this._lock.on('authenticated', (authResult) => {
      this._lock.hide();
      const formData = new FormData();
      Axios({
        method: 'post',
        url: '/api/auth/login',
        data: {
          accessToken: authResult.accessToken,
        },
      }).then((response) => {
        this.setSession({
          idToken: response.data.Token,
          expiresAt: response.data.ExpiresAt,
        });
      });
    });
  }

  componentWillUnmount() {
    this._lock.hide();
  }

  setSession = (authResult) => {
    const expiresAt = authResult.expiresAt * 1000;

    localStorage.setItem('auth', JSON.stringify({
      access_token: authResult.accessToken,
      id_token: authResult.idToken,
      expires_at: expiresAt,
    }));
  }

  isAuthenticated = () => {
    // Check whether the current time is past the
    // Access Token's expiry time.
    const storedAuth = JSON.parse(localStorage.getItem('auth'));
    const expiresAt = storedAuth && storedAuth.expires_at;
    return new Date().getTime() < expiresAt;
  }

  render() {
    return <div id={this.props.containerID}/>;
  }
}
