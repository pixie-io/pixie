import auth0 from 'auth0-js';

const AUTH0_DOMAIN = 'pixie-labs.auth0.com';
const AUTH0_CLIENT_ID = 'qaAfEHQT7mRt6W0gMd9mcQwNANz9kRup';

interface AuthConfig {
  domain?: string;
  clientID?: string;
}

export class Auth {
  auth0: auth0.WebAuth;
  constructor(config: AuthConfig) {
    this.auth0 = new auth0.WebAuth({
      domain: config.domain || AUTH0_DOMAIN,
      clientID: config.clientID || AUTH0_CLIENT_ID,
      redirectUri: window.location.origin + '/callback',
      responseType: 'token id_token',
      scope: 'openid',
    });
  }

  login = () => {
    this.auth0.authorize();
  }

  handleAuthentication = () => {
    this.auth0.parseHash((err, authResult) => {
      if (authResult && authResult.accessToken && authResult.idToken) {
        this.setSession(authResult);
        window.location.href = '/';
      }
    });
  }

  setSession = (authResult) => {
    // Set the time that the Access Token will expire at.
    const expiresAt = JSON.stringify((authResult.expiresIn * 1000) + new Date().getTime());

    localStorage.setItem('auth', JSON.stringify({
      access_token: authResult.accessToken,
      id_token: authResult.idToken,
      expires_at: expiresAt,
    }));
  }

  logout = () => {
    // Clear Access Token and ID Token from local storage.
    localStorage.removeItem('auth');
  }

  isAuthenticated = () => {
    // Check whether the current time is past the
    // Access Token's expiry time.
    const storedAuth = JSON.parse(localStorage.getItem('auth'));
    const expiresAt = storedAuth && storedAuth.expires_at;
    return new Date().getTime() < expiresAt;
  }
}
