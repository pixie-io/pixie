import Auth0Lock from 'auth0-lock';
import Axios from 'axios';
import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {AUTH0_CLIENT_ID, AUTH0_DOMAIN, DOMAIN_NAME} from 'containers/constants';
import gql from 'graphql-tag';
// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/dark-logo.svg';
// @ts-ignore : TS does not like image files.
import * as criticalImage from 'images/icons/critical.svg';
import * as QueryString from 'query-string';
import * as React from 'react';
import {ApolloConsumer} from 'react-apollo';
import {Button} from 'react-bootstrap';
import analytics from 'utils/analytics';
import * as RedirectUtils from 'utils/redirect-utils';

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

interface Auth0LoginState {
  error: string;
  token: string;
}

function onLoginAuthenticated(authResult) {
  this._lock.getUserInfo(authResult.accessToken, (error, profile) => {
    this._lock.hide();
    if (this.cliAuthMode === 'manual') {
      this.setState({
        token: authResult.accessToken,
      });
      return;
    }

    return Axios({
      method: 'post',
      url: '/api/auth/login',
      data: {
        accessToken: authResult.accessToken,
        userEmail: profile.email,
        siteName: this.siteName,
      },
    }).then((response) => {
      this.setSession({
        idToken: response.data.token,
        expiresAt: response.data.expiresAt,
      });

      // Associate anonymous use with actual user ID.
      analytics.identify(response.data.userInfo.userID, {});
      if (response.data.userCreated) {
        analytics.track('User created');
      }
      analytics.track('Login success');

      RedirectUtils.redirect(this.siteName, this.redirectPath || '/', {});
    }).catch((err) => {
      analytics.track('Login failed', { error: err.response.data });
      this.setState({
        error: err.response.data,
      });
    });
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
        siteName: this.siteName,
      },
    }).then((response) => {
      this.setSession({
        idToken: response.data.token,
        expiresAt: response.data.expiresAt,
      });
      // Associate anonymous use with actual user ID.
      analytics.identify(response.data.userInfo.userID, {});
      analytics.track('Login success');
      analytics.track('Site created', { siteName: this.siteName });

    }).then((results) => {
      RedirectUtils.redirect(this.siteName, this.redirectPath || '/', {});
    }).catch((err) => {
      analytics.track('Site create failed', { error: err.response.data });
      this.setState({
        error: err.response.data,
      });
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

export class Auth0Login extends React.Component<Auth0LoginProps, Auth0LoginState>  {
  public static defaultProps: Partial<Auth0LoginProps> = {
    containerID: 'pl-auth0-lock-container',
  };

  private _lock: Auth0LockStatic;
  private siteName: string;
  private auth0Redirect: string;
  private redirectPath: string;
  private cliAuthMode: '' | 'auto' | 'manual' = '';
  private noCache = false;
  private responseMode: string;

  constructor(props) {
    super(props);

    this.state = {
      error: '',
      token: '',
    };
  }

  parseQueryParams() {
    const queryParams = QueryString.parse(this.props.location.search);

    const locationParam = typeof queryParams.location === 'string' ? queryParams.location : '';
    const siteName = typeof queryParams.site_name === 'string' ? queryParams.site_name : '';
    const localMode = typeof queryParams.local_mode === 'string' ? queryParams.local_mode : '';
    const localModeRedirect = typeof queryParams.redirect_uri === 'string' ? queryParams.redirect_uri : '';

    this.noCache = typeof queryParams.no_cache === 'string' && queryParams.no_cache === 'true';
    if (this.noCache) {
      this.clearSession();
    }
    this.siteName = siteName;
    this.redirectPath = locationParam;

    // Default redirect URL.
    this.auth0Redirect = window.location.origin + '/' + this.props.redirectPath + '?site_name=' + this.siteName;
    if (locationParam !== '') {
      this.auth0Redirect = this.auth0Redirect + '&location=' + locationParam;
    }
    this.responseMode = '';

    // If localMode is on, redirect to the given location.
    if (localMode === 'true' && localModeRedirect !== '') {
      this.cliAuthMode = 'auto';
      this.auth0Redirect = localModeRedirect;
      this.responseMode = 'form_post';
    } else if (localMode === 'true') {
      this.cliAuthMode = 'manual';
      this.auth0Redirect = this.auth0Redirect + '&local_mode=true';
    }
  }

  componentDidMount() {
    this.parseQueryParams();

    // Redirect to the correct login endpoint if the path is incorrect.
    const subdomain = window.location.host.split('.')[0];
    if (subdomain !== 'id') {
      RedirectUtils.redirect('id', window.location.pathname, { ['site_name']: subdomain });
    }

    this._lock = new Auth0Lock(AUTH0_CLIENT_ID, AUTH0_DOMAIN, {
      auth: {
        redirectUrl: this.auth0Redirect,
        responseType: 'token',
        responseMode: this.responseMode,
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
        title: this.siteName,
        signUpTitle: this.siteName,
        signUpTerms: 'By signing up, I agree to the Terms of Services and Privacy Policy',
      },
      allowedConnections: ['google-oauth2'],
    });

    if (!this.cliAuthMode && !this.noCache && this.isAuthenticated()) {
      RedirectUtils.redirect(this.siteName, this.redirectPath || '/', {});
    }

    const options = this.cliAuthMode || this.noCache ?
      { auth: { params: { prompt: 'select_account' } } } as Auth0LockShowOptions :
      null;
    this._lock.show(options);
    this._lock.on('authenticated', (auth) => {
      analytics.track('Auth success');
      this.props.onAuthenticated.call(this, auth);
    });
    this._lock.on('signin ready', () => analytics.track('Auth start'));
    this._lock.on('authorization_error', (error) => analytics.track('Auth failed', { error }));
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

  clearSession() {
    localStorage.removeItem('auth');
  }

  isAuthenticated = () => {
    // Check whether the current time is past the
    // Access Token's expiry time.
    const storedAuth = JSON.parse(localStorage.getItem('auth'));
    const expiresAt = storedAuth && storedAuth.expires_at;
    return new Date().getTime() < expiresAt;
  }

  render() {
    if (this.state.token !== '') {
      return (
        <DialogBox width={480}>
          Please copy this code, switch to the CLI and paste it there:
          <CodeSnippet showCopy={true} language=''>
            {this.state.token}
          </CodeSnippet>
        </DialogBox>
      );
    }

    if (this.state.error === '') {
      return <div id={this.props.containerID} />;
    }

    return (
      <DialogBox width={480}>
        <div className='error-message'>
          <div className='error-message--icon'><img src={criticalImage} /></div>
          {this.state.error}
        </div>
        <Button variant='danger' onClick={() => {
          // Just reload page to clear state and allow them to retry login.
          document.location.reload();
        }}>
          Retry
        </Button>
      </DialogBox>
    );
  }
}
