import Auth0Lock from 'auth0-lock';
import Axios from 'axios';
import {AUTH0_CLIENT_ID, AUTH0_DOMAIN, DOMAIN_NAME} from 'containers/constants';
import gql from 'graphql-tag';
import * as QueryString from 'query-string';
import * as React from 'react';
import { ApolloConsumer } from 'react-apollo';

// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/dark-logo.svg';

export interface RouterInfo {
  search: string;
}

interface Auth0LoginProps {
  containerID?: string;
  location: RouterInfo;
  redirectPath: string;
  onAuthenticated: (authResult) => void;
  allowSignUp: boolean;
  allowLogin: boolean;
}

function onLoginAuthenticated(authResult) {
  this._lock.hide();
  return Axios({
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
    window.location.href = window.location.protocol + '//' + this.domain + '.' +
      DOMAIN_NAME + '/vizier/query';
  });
}

function onCreateAuthenticated(authResult) {
  this._lock.getUserInfo(authResult.accessToken, (error, profile) => {
    this._lock.hide();
    return Axios({
      method: 'post',
      url: '/api/create-site',
      data: {
        accessToken: authResult.accessToken,
        userEmail: profile.email,
        domainName: this.domain,
      },
    }).then((response) => {
      this.setSession({
        idToken: response.data.Token,
        expiresAt: response.data.ExpiresAt,
      });
    }).then((results) => {
        window.location.href = window.location.protocol + '//' + this.domain + '.' +
          DOMAIN_NAME + '/vizier/query';
    }).catch((gqlErr) => {
        return gqlErr;
    });
});
}

export const UserLogin = (props) => {
  return (<Auth0Login
      redirectPath='login'
      onAuthenticated={onLoginAuthenticated}
      allowLogin={true}
      allowSignUp={false}
      {...props}
    />
  );
};

export const UserCreate = (props) => {
  return (<ApolloConsumer>{(client) => {
    return (<Auth0Login
        client={client}
        redirectPath='create-site'
        onAuthenticated={onCreateAuthenticated}
        allowLogin={false}
        allowSignUp={true}
        {...props}
      />
    );
  }}</ApolloConsumer>);
};

class Auth0Login extends React.Component<Auth0LoginProps, {}>  {
  public static defaultProps: Partial<Auth0LoginProps> = {
    containerID: 'pl-auth0-lock-container',
  };

  private _lock: Auth0LockStatic;
  private domain: string | string[];

  componentDidMount() {
    this.domain = QueryString.parse(this.props.location.search).domain_name;

    this._lock = new Auth0Lock(AUTH0_CLIENT_ID, AUTH0_DOMAIN, {
      auth: {
        redirectUrl: window.location.origin + '/' + this.props.redirectPath + '?domain_name=' + this.domain,
        responseType: 'token',
        params: {
          scope: 'openid profile user_metadata email',
        },
      },
      allowSignUp: this.props.allowSignUp,
      allowLogin: this.props.allowLogin,
      container: this.props.containerID,
      initialScreen: 'login',
      theme: {
        logo: logoImage,
      },
      languageDictionary: {
        title: this.domain,
        signUpTitle: this.domain,
        signUpTerms: 'By signing up, I agree to the Terms of Services and Privacy Policy',
      },
      allowedConnections: ['google-oauth2'],
    });

    if (this.isAuthenticated()) {
      this._lock.hide();
      return;
    }

    this._lock.show();
    this._lock.on('authenticated', this.props.onAuthenticated.bind(this));
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
