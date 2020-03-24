import Auth0Lock from 'auth0-lock';
import Axios from 'axios';
import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {AUTH0_CLIENT_ID, AUTH0_DOMAIN, DOMAIN_NAME} from 'containers/constants';
// @ts-ignore : TS does not like image files.
import * as check from 'images/icons/check.svg';
// @ts-ignore : TS does not like image files.
import * as criticalImage from 'images/icons/critical.svg';
// @ts-ignore : TS does not like image files.
import * as backgroundBottom from 'images/login-background-bottom.svg';
// @ts-ignore : TS does not like image files.
import * as backgroundTop from 'images/login-background-top.svg';
// @ts-ignore : TS does not like image files.
import * as logo from 'images/new-logo.svg';
import * as QueryString from 'query-string';
import * as React from 'react';
import {ApolloConsumer} from 'react-apollo';
import {Button} from 'react-bootstrap';
import {Link} from 'react-router-dom';
import analytics from 'utils/analytics';
import * as RedirectUtils from 'utils/redirect-utils';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    container: {
      backgroundColor: theme.palette.background.four,
      color: 'white',
      width: '45%',
      height: '100%',
    },
    logo: {
      padding: theme.spacing(8),
      width: '14rem',
    },
    background: {
      padding: theme.spacing(1),
      position: 'absolute',
      zIndex: 1,
      width: '45%',
      minHeight: '42rem',
      height: '100%',
    },
    backgroundTop: {
      height: '50%',
      backgroundSize: '12rem',
      backgroundRepeat: 'no-repeat',
      backgroundPositionX: 'right',
      backgroundPositionY: 'top',
      backgroundImage: `url(${backgroundTop})`,
    },
    backgroundBottom: {
      height: '50%',
      backgroundSize: '18rem',
      backgroundRepeat: 'no-repeat',
      backgroundPositionX: 'right',
      backgroundPositionY: 'bottom',
      backgroundImage: `url(${backgroundBottom})`,
    },
    contents: {
      position: 'relative',
      zIndex: 2,
      paddingTop: theme.spacing(4.5),
      paddingLeft: theme.spacing(12),
      paddingRight: theme.spacing(25),
    },
    header: {
      ...theme.typography.h4,
    },
    subheader: {
      ...theme.typography.overline,
      fontWeight: 500,
      fontSize: '14px',
      color: '#FF5E6D',
      letterSpacing: '0.3em',
    },
    text: {
      ...theme.typography.subtitle1,
      fontSize: '22px',
      paddingTop: theme.spacing(8),
      paddingBottom: theme.spacing(3),
    },
    bullet: {
      ...theme.typography.body1,
      fontSize: '18px',
      color: '#9696A5',
      paddingTop: theme.spacing(3),
      '&::before': {
        content: '""',
        display: 'inline-block',
        height: '1.2rem',
        width: '1.2rem',
        backgroundImage: `url(${check})`,
        backgroundSize: 'contain',
        backgroundRepeat: 'no-repeat',
        marginRight: theme.spacing(1),
      },
    },
    check: {
      paddingRight: theme.spacing(1),
      height: '1rem',
    },
    footer: {
      ...theme.typography.body2,
      position: 'absolute',
      bottom: 0,
      padding: theme.spacing(1),
      color: '#596274',
    },
    list: {
      listStyle: 'none',
    },
  });
});

const CompanyInfo = () => {
  const classes = useStyles();
  const theme = useTheme();

  return (
    <div className={classes.container}>
      <div className={classes.background}>
        <div className={classes.backgroundTop} />
        <div className={classes.backgroundBottom} />
      </div>
      <img className={classes.logo} src={logo} />
      <div className={classes.contents}>
        <div className={classes.header}>Pixie Community</div>
        <div className={classes.subheader}>Private Beta</div>
        <div className={classes.text}>
          Engineers use Pixie's auto-telemetry to debug distributed environments in real-time
          </div>
        <ul className={classes.list}>
          <li className={classes.bullet}>
            Unlimited early access to core features
            </li>
          <li className={classes.bullet}>
            Support via community Slack & Github
            </li>
          <li className={classes.bullet}>
            No credit card needed
            </li>
        </ul>
      </div>
      <div className={classes.footer}>
        <span>&#169;</span> 2020, Pixie Labs Inc.
      </div>
    </div>
  );
};

export const UserLogin = (props) => {
  return (
    <LoginContainer
      signUp={false}
      headerText={'Log-in to Pixie'}
      footerText={'Not signed up?'}
      footerLinkText={'Create an account here'}
      footerLink={'/signup'}
      {...props}
    />
  );
};

export const UserCreate = (props) => {
  return (
    <LoginContainer
      signUp={true}
      headerText={'Create your account'}
      footerText={'Already signed up?'}
      footerLinkText={'Log in here'}
      footerLink={'/login'}
      {...props}
    />
  );
};

export interface RouterInfo {
  search: string;
}

interface LoginProps {
  signUp: boolean;
  location: RouterInfo;
  headerText: string;
  footerText: string;
  footerLinkText: string;
  footerLink: string;
}

interface LoginState {
  error: string;
  token: string;
}

function onLoginAuthenticated(authResult, profile, error) {
  const traits = {
    email: profile.email,
    firstName: profile.firstName,
    lastName: profile.lastName,
  };
  analytics.identify('', traits);
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
    },
  }).then((response) => {
    // Associate anonymous use with actual user ID.
    analytics.identify(response.data.userInfo.userID, traits);
    if (response.data.userCreated) {
      analytics.track('User created');
    }
    analytics.track('Login success');

    RedirectUtils.redirect(this.redirectPath || '/', {});
  }).catch((err) => {
    analytics.track('Login failed', { error: err.response.data });
    this.setState({
      error: err.response.data,
    });
  });
}

function onCreateAuthenticated(authResult, profile, error) {
  const traits = {
    email: profile.email,
    firstName: profile.firstName,
    lastName: profile.lastName,
  };
  analytics.identify('', traits);
  return Axios({
    method: 'post',
    url: '/api/auth/signup',
    data: {
      accessToken: authResult.accessToken,
      userEmail: profile.email,
    },
  }).then((response) => {
    // Associate anonymous use with actual user ID.
    analytics.identify(response.data.userInfo.userID, traits);
    analytics.track('User signed up');
  }).then((results) => {
    RedirectUtils.redirect(this.redirectPath || '/', {});
  }).catch((err) => {
    analytics.track('User signup failed', { error: err.response.data });
    this.setState({
      error: err.response.data,
    });
  });
}

export class LoginContainer extends React.Component<LoginProps, LoginState> {
  private noCache = false;
  private responseMode: '' | 'form_post';
  private cliAuthMode: '' | 'auto' | 'manual' = '';
  private auth0Redirect: string;
  private redirectPath: string;
  private authenticating: boolean;

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
    const localMode = typeof queryParams.local_mode === 'string' ? queryParams.local_mode : '';
    const localModeRedirect = typeof queryParams.redirect_uri === 'string' ? queryParams.redirect_uri : '';
    const authenticatingParam = typeof queryParams.authenticating === 'string' ? queryParams.authenticating : '';
    this.authenticating = window.location.hash.search('access_token') !== -1;

    this.noCache = typeof queryParams.no_cache === 'string' && queryParams.no_cache === 'true';
    this.redirectPath = locationParam;

    // Default redirect URL.
    this.auth0Redirect = window.location.origin + (this.props.signUp ? '/signup' : '/login');
    if (locationParam !== '') {
      this.auth0Redirect = this.auth0Redirect + '?location=' + locationParam;
    }
    this.responseMode = '';

    // If localMode is on, redirect to the given location.
    if (localMode === 'true' && localModeRedirect !== '') {
      this.cliAuthMode = 'auto';
      this.auth0Redirect = localModeRedirect;
      this.responseMode = 'form_post';
    } else if (localMode === 'true') {
      this.cliAuthMode = 'manual';
      this.auth0Redirect = this.auth0Redirect + '?local_mode=true';
    }
  }

  componentWillMount() {
    this.parseQueryParams();
  }

  render() {
    let showCompanyInfo = !this.authenticating;
    let loginBody = (
      <>
        {this.authenticating ? null : <div className='login-header'>{this.props.headerText}</div>}
        <Auth0Login
          redirectPath={this.auth0Redirect}
          allowSignUp={this.props.signUp}
          allowLogin={!this.props.signUp}
          responseMode={this.responseMode}
          onAuthenticated={this.props.signUp ?
            onCreateAuthenticated.bind(this) : onLoginAuthenticated.bind(this)}
        />
        {this.authenticating ? null :
          <span className='login-footer'>{this.props.footerText + ' '}
            <Link style={{ textDecoration: 'underline' }} to={this.props.footerLink}>{this.props.footerLinkText}</Link>
          </span>}
      </>
    );

    if (this.state.token !== '') {
      showCompanyInfo = false;
      loginBody = (
        <DialogBox width={480}>
          Please copy this code, switch to the CLI and paste it there:
          <CodeSnippet showCopy={true} language=''>
            {this.state.token}
          </CodeSnippet>
        </DialogBox>
      );
    } else if (this.state.error !== '') {
      showCompanyInfo = false;
      loginBody = (
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

    return (
      <>
        {showCompanyInfo ? <CompanyInfo /> : null}
        <div className='center-content' style={{ flexDirection: 'column', flex: 1 }}>
          {loginBody}
        </div>
      </>
    );
  }
}

interface Auth0LoginProps {
  containerID?: string;
  redirectPath: string; // Path that auth0 should redirect to after authentication.
  onAuthenticated: (authResult, profile, error) => void;
  allowSignUp: boolean;
  allowLogin: boolean;
  responseMode: '' | 'form_post';
}

function makeStyledLink(href: string, title: string): string {
  return `<a target="_blank" style="color: #409aff; text-decoration: underline" href=${href}>${title}</a>`;
}

const TERMS_OF_USE_LINK = makeStyledLink('https://pixielabs.ai/terms', 'Terms of Use');
const PRIVACY_POLICY_LINK = makeStyledLink('https://pixielabs.ai/privacy', 'Privacy Policy');
const COOKIE_POLICY_LINK = makeStyledLink('https://pixielabs.ai/cookies', 'Cookie Policy');

class Auth0Login extends React.Component<Auth0LoginProps>  {
  public static defaultProps: Partial<Auth0LoginProps> = {
    containerID: 'pl-auth0-lock-container',
  };

  private _lock: Auth0LockStatic;

  componentDidMount() {
    this._lock = new Auth0Lock(AUTH0_CLIENT_ID, AUTH0_DOMAIN, {
      auth: {
        redirectUrl: this.props.redirectPath,
        responseType: 'token',
        responseMode: this.props.responseMode,
        params: {
          scope: 'openid profile user_metadata email',
        },
      },
      allowSignUp: this.props.allowSignUp,
      allowLogin: this.props.allowLogin,
      container: this.props.containerID,
      initialScreen: 'login',
      languageDictionary: {
        signUpTerms:
          `By signing up, you agree to the ${TERMS_OF_USE_LINK}, ${PRIVACY_POLICY_LINK}, and ${COOKIE_POLICY_LINK}.`,
      },
      allowedConnections: ['google-oauth2'],
    });

    const options =
      { auth: { params: { prompt: 'select_account' } } } as Auth0LockShowOptions;

    this._lock.show(options);
    this._lock.on('authenticated', (auth) => {
      analytics.track('Auth success');
      this._lock.getUserInfo(auth.accessToken, (error, profile) => {
        this._lock.hide();
        this.props.onAuthenticated(auth, profile, error);
      });
    });
    this._lock.on('signin ready', () => analytics.track('Auth start'));
    this._lock.on('authorization_error', (error) => analytics.track('Auth failed', { error }));
  }

  componentWillUnmount() {
    this._lock.hide();
  }

  render() {
    return (
      <div id={this.props.containerID} />
    );
  }
}
