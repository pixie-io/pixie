import Auth0Lock from 'auth0-lock';
import Axios from 'axios';
import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {AUTH0_CLIENT_ID, AUTH0_DOMAIN, DOMAIN_NAME} from 'containers/constants';
import gql from 'graphql-tag';
import * as QueryString from 'query-string';
import * as React from 'react';
import { ApolloConsumer } from 'react-apollo';
import {Button} from 'react-bootstrap';
import * as RedirectUtils from 'utils/redirect-utils';

// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/dark-logo.svg';
// @ts-ignore : TS does not like image files.
import * as criticalImage from 'images/icons/critical.svg';

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
    if (this.localMode) {
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
        domainName: this.domain,
      },
    }).then((response) => {
      this.setSession({
        idToken: response.data.Token,
        expiresAt: response.data.ExpiresAt,
      });
      RedirectUtils.redirect(this.domain, this.redirectPath || '/vizier/query', {});
    }).catch((err) => {
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
        domainName: this.domain,
      },
    }).then((response) => {
      this.setSession({
        idToken: response.data.Token,
        expiresAt: response.data.ExpiresAt,
      });
    }).then((results) => {
        RedirectUtils.redirect(this.domain, this.redirectPath || '/vizier/query', {});
    }).catch((err) => {
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
  private domain: string;
  private auth0Redirect: string;
  private redirectPath: string;
  private localMode: boolean;
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
    const domainName = typeof queryParams.domain_name === 'string' ? queryParams.domain_name : '';
    const localMode = typeof queryParams.local_mode === 'string' ? queryParams.local_mode : '';
    const localModeRedirect = typeof queryParams.redirect_uri === 'string' ? queryParams.redirect_uri : '';

    this.domain = domainName;
    this.redirectPath = locationParam;

    // Default redirect URL.
    this.auth0Redirect = window.location.origin + '/' + this.props.redirectPath + '?domain_name=' + this.domain;
    if (locationParam !== '') {
      this.auth0Redirect = this.auth0Redirect + '&location=' + locationParam;
    }
    this.responseMode = '';

    // If localMode is on, redirect to the given location.
    if (localMode === 'true' && localModeRedirect !== '') {
      this.auth0Redirect = localModeRedirect;
      this.responseMode = 'form_post';
    } else if (localMode === 'true') {
      this.localMode = true;
      this.auth0Redirect = this.auth0Redirect + '&local_mode=true';
    }
  }

  componentDidMount() {
    this.parseQueryParams();

    // Redirect to the correct login endpoint if the path is incorrect.
    const subdomain = window.location.host.split('.')[0];
    if (subdomain !== 'id') {
      RedirectUtils.redirect('id', window.location.pathname, {['domain_name']: subdomain});
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
      return <div id={this.props.containerID}/>;
    }

    return (
      <DialogBox width={480}>
        <div className='error-message'>
          <div className='error-message--icon'><img src={criticalImage}/></div>
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
