import auth0 from 'auth0-js';
import {
  AUTH0_CLIENT_ID, AUTH0_DOMAIN,
} from 'containers/constants';
import { OAuthProviderClient, Token } from './oauth-provider';

function makeAuth0Client(): auth0.WebAuth {
  return new auth0.WebAuth({
    domain: AUTH0_DOMAIN,
    clientID: AUTH0_CLIENT_ID,
  });
}

export class Auth0Client extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
  }

  loginRequest() {
    makeAuth0Client().authorize({
      connection: 'google-oauth2',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ false),
      prompt: 'login',
    });
  }

  signupRequest() {
    makeAuth0Client().authorize({
      connection: 'google-oauth2',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ true),
      prompt: 'login',
    });
  }

  // eslint-disable-next-line class-methods-use-this
  handleToken(): Promise<Token> {
    return new Promise<Token>((resolve, reject) => {
      makeAuth0Client().parseHash({ hash: window.location.hash }, (errStatus, authResult) => {
        if (errStatus) {
          reject(new Error(`${errStatus.error} - ${errStatus.errorDescription}`));
          return;
        }
        resolve(authResult.accessToken);
      });
    });
  }
}
