import {
  AUTH_URI, AUTH_CLIENT_ID,
} from 'containers/constants';
import ClientOAuth2 from 'client-oauth2';
import { OAuthProviderClient, Token } from './oauth-provider';

// Copied from auth0-js/src/helper/window.js
function randomString(length) {
  // eslint-disable-next-line
  var bytes = new Uint8Array(length);
  const result = [];
  const charset = '0123456789ABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyz-._~';

  const cryptoObj = window.crypto;
  if (!cryptoObj) {
    return null;
  }

  const random = cryptoObj.getRandomValues(bytes);

  for (let a = 0; a < random.length; a++) {
    result.push(charset[random[a] % charset.length]);
  }

  return result.join('');
}

const hydraStorageKey = 'hydra_auth_state';
export class HydraClient extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  hydraStorageKey: string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
    this.hydraStorageKey = hydraStorageKey;
  }

  loginRequest() {
    window.location.href = this.makeClient(this.makeAndStoreState(), /* isSignup */ false).token.getUri();
  }

  signupRequest() {
    window.location.href = this.makeClient(this.makeAndStoreState(), /* isSignup */ true).token.getUri();
  }

  handleToken(): Promise<Token> {
    return new Promise<Token>((resolve, reject) => {
      this.makeClient(this.getStoredState(), false).token.getToken(window.location).then((user) => {
        resolve(user.accessToken);
      }).catch((err) => reject(err));
    });
  }

  private makeAndStoreState(): string {
    const state = randomString(48);
    window.localStorage.setItem(this.hydraStorageKey, state);
    return state;
  }

  private getStoredState(): string {
    const state = window.localStorage.getItem(this.hydraStorageKey);
    if (state != null) {
      return state;
    }

    throw new Error('OAuth state not found. Please try logging in again.');
  }

  private makeClient(state: string, isSignup: boolean): ClientOAuth2 {
    return new ClientOAuth2({
      clientId: AUTH_CLIENT_ID,
      authorizationUri: `https://${window.location.host}/${AUTH_URI}`,
      redirectUri: this.getRedirectURL(isSignup),
      scopes: ['vizier'],
      state,
    });
  }
}
