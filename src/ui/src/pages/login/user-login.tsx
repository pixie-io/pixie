// eslint-disable-next-line max-classes-per-file
import Auth0Lock from 'auth0-lock';
import Axios from 'axios';
import CodeRenderer from 'components/code-renderer/code-renderer';
import { DialogBox } from 'components/dialog-box/dialog-box';
import { AUTH0_CLIENT_ID, AUTH0_DOMAIN } from 'containers/constants';
import * as check from 'images/icons/check.svg';
import * as criticalImage from 'images/icons/critical.svg';
import * as backgroundBottom from 'images/login-background-bottom.svg';
import * as backgroundTop from 'images/login-background-top.svg';
import * as logo from 'images/new-logo.svg';
import * as QueryString from 'query-string';
import * as React from 'react';
import Button from '@material-ui/core/Button';
import { Link } from 'react-router-dom';
import analytics from 'utils/analytics';
import * as RedirectUtils from 'utils/redirect-utils';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

const useStyles = makeStyles((theme: Theme) => createStyles({
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
}));

const CompanyInfo = () => {
  const classes = useStyles();

  return (
    <div className={classes.container}>
      <div className={classes.background}>
        <div className={classes.backgroundTop} />
        <div className={classes.backgroundBottom} />
      </div>
      <img className={classes.logo} src={logo} />
      <div className={classes.contents}>
        <div className={classes.header}>Pixie Community</div>
        <div className={classes.subheader}>Beta</div>
        <div className={classes.text}>
          Engineers use Pixie&apos;s auto-telemetry to debug distributed environments in real-time
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
        <span>&#169;</span>
        {' '}
        2020, Pixie Labs Inc.
      </div>
    </div>
  );
};

export const UserLogin = (props) => (
  <LoginContainer
    signUp={false}
    headerText='Log-in to Pixie'
    footerLinkText='Sign up for a new account'
    footerLink='/signup'
    {...props}
  />
);

export const UserCreate = (props) => (
  <LoginContainer
    signUp
    headerText='Create your account'
    footerLinkText='Log into an existing account'
    footerLink='/login'
    {...props}
  />
);

export interface RouterInfo {
  search: string;
}

interface LoginProps {
  signUp: boolean;
  location: RouterInfo;
  headerText: string;
  footerLinkText: string;
  footerLink: string;
}

interface LoginState {
  error: string;
  token: string;
}

function redirectPost(url, data) {
  const form = document.createElement('form');
  document.body.appendChild(form);
  form.method = 'post';
  form.action = url;
  // eslint-disable-next-line guard-for-in
  for (const name in data) {
    const input = document.createElement('input');
    input.type = 'hidden';
    input.name = name;
    input.value = data[name];
    form.appendChild(input);
  }
  form.submit();
}

function onLoginAuthenticated(authResult, profile /* error */) {
  // TODO(malthus): Handle the error.
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

  if (this.tokenPostPath) {
    // eslint-disable-next-line @typescript-eslint/camelcase
    redirectPost(this.tokenPostPath, { access_token: authResult.accessToken, user_email: profile.email });
    return;
  }

  // eslint-disable-next-line consistent-return
  return Axios({
    method: 'post',
    url: '/api/auth/login',
    data: {
      accessToken: authResult.accessToken,
      userEmail: profile.email,
      orgName: this.orgName,
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

function onCreateAuthenticated(authResult, profile /* error */) {
  // TODO(malthus): Handle the error.
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
  }).then(() => {
    if (this.tokenPostPath) {
      // eslint-disable-next-line @typescript-eslint/camelcase
      redirectPost(this.tokenPostPath, { access_token: authResult.accessToken, user_email: profile.email });
      return;
    }

    if (this.cliAuthMode !== 'manual') {
      RedirectUtils.redirect(this.redirectPath || '/', {});
    } else {
      this.setState({
        token: authResult.accessToken,
      });
    }
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

  private tokenPostPath: string;

  private authenticating: boolean;

  private orgName: string;

  constructor(props) {
    super(props);

    this.state = {
      error: '',
      token: '',
    };
  }

  UNSAFE_componentWillMount() {
    this.parseQueryParams();
  }

  parseQueryParams() {
    const queryParams = QueryString.parse(this.props.location.search);

    const locationParam = typeof queryParams.location === 'string' ? queryParams.location : '';
    const localMode = typeof queryParams.local_mode === 'string' ? queryParams.local_mode : '';
    const localModeRedirect = typeof queryParams.redirect_uri === 'string' ? queryParams.redirect_uri : '';
    this.orgName = typeof queryParams.org_name === 'string' ? queryParams.org_name : '';
    this.authenticating = window.location.hash.search('access_token') !== -1;

    const segmentId = typeof queryParams.tid === 'string' ? queryParams.tid : '';
    if (segmentId) {
      analytics.alias(segmentId);
    }

    this.noCache = typeof queryParams.no_cache === 'string' && queryParams.no_cache === 'true';
    this.redirectPath = locationParam;

    // Default redirect URL.
    this.auth0Redirect = window.location.origin + (this.props.signUp ? '/signup' : '/login');
    if (locationParam !== '') {
      this.auth0Redirect = `${this.auth0Redirect}?location=${locationParam}`;
    }
    this.responseMode = '';

    // If localMode is on, redirect to the given location.
    if (localMode === 'true' && localModeRedirect !== '') {
      this.cliAuthMode = 'auto';
      this.tokenPostPath = localModeRedirect;
      this.auth0Redirect = `${this.auth0Redirect}?local_mode=true&redirect_uri=${localModeRedirect}`;
    } else if (localMode === 'true') {
      this.cliAuthMode = 'manual';
      this.auth0Redirect += '?local_mode=true';
    }

    if (this.orgName !== '') {
      const queryStr = this.auth0Redirect.includes('?') ? '&' : '?';
      this.auth0Redirect = `${this.auth0Redirect + queryStr}org_name=${this.orgName}`;
    }
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
          /* eslint-disable-next-line react/jsx-no-bind */
          onAuthenticated={this.props.signUp
            ? onCreateAuthenticated.bind(this) : onLoginAuthenticated.bind(this)}
        />
        {this.authenticating ? null
          : (
            <span className='login-footer'>
              <hr />
              <Link to={{
                pathname: this.props.footerLink,
                search: this.props.location.search,
              }}
              >
                {this.props.footerLinkText}
              </Link>
            </span>
          )}
      </>
    );

    if (this.state.token !== '') {
      showCompanyInfo = false;
      loginBody = (
        <DialogBox width={480}>
          Please copy this code, switch to the CLI and paste it there:
          <CodeRenderer
            code={this.state.token}
          />
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
          <Button
            variant='contained'
            color='secondary'
            onClick={() => {
              // Just reload page to clear state and allow them to retry login.
              document.location.reload();
            }}
          >
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

class Auth0Login extends React.Component<Auth0LoginProps> {
  // eslint-disable-next-line react/static-property-placement
  public static defaultProps: Partial<Auth0LoginProps> = {
    containerID: 'pl-auth0-lock-container',
  };

  private lock: Auth0LockStatic;

  componentDidMount() {
    this.lock = new Auth0Lock(AUTH0_CLIENT_ID, AUTH0_DOMAIN, {
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

    const options = { auth: { params: { prompt: 'select_account' } } } as Auth0LockShowOptions;

    this.lock.show(options);
    this.lock.on('authenticated', (auth) => {
      analytics.track('Auth success');
      this.lock.getUserInfo(auth.accessToken, (error, profile) => {
        this.lock.hide();
        this.props.onAuthenticated(auth, profile, error);
      });
    });
    this.lock.on('signin ready', () => analytics.track('Auth start'));
    this.lock.on('authorization_error', (error) => analytics.track('Auth failed', { error }));
  }

  componentWillUnmount() {
    this.lock.hide();
  }

  render() {
    return (
      <div id={this.props.containerID} />
    );
  }
}
