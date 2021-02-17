import auth0 from 'auth0-js';
import { AUTH0_CLIENT_ID, AUTH0_DOMAIN } from 'containers/constants';
import * as QueryString from 'query-string';

export type AuthCallbackMode = 'cli_get' | 'cli_token' | 'ui';

interface RedirectArgs {
  mode?: AuthCallbackMode;
  location?: string;
  org_name?: string;
  signup?: boolean;
  redirect_uri?: string;
}

const auth0Request = (isSignup: boolean) => {
  const wa = new auth0.WebAuth({
    domain: AUTH0_DOMAIN,
    clientID: AUTH0_CLIENT_ID,
  });

  // Translate the old API parameters to new versions. In paricular:
  // local, (no redirect_url) -> cli_token
  // local -> cli_get
  // default: ui
  // We also translate the location parameters so redirects work as expected.
  // TODO(zasgar/michelle): When we finish porting everything to the new API this code
  // can be simplified.

  const redirectArgs: RedirectArgs = {};
  const parsed = QueryString.parse(window.location.search.substring(1));
  if (parsed.local_mode && !!parsed.local_mode) {
    if (parsed.redirect_uri) {
      // eslint-disable-next-line @typescript-eslint/camelcase
      redirectArgs.redirect_uri = typeof parsed.redirect_uri === 'string' && String(parsed.redirect_uri);
      redirectArgs.mode = 'cli_get';
    } else {
      redirectArgs.mode = 'cli_token';
    }
  } else {
    if (parsed.redirect_uri && typeof parsed.redirect_uri === 'string') {
      // eslint-disable-next-line @typescript-eslint/camelcase
      redirectArgs.redirect_uri = String(parsed.redirect_uri);
    }
    redirectArgs.mode = 'ui';
  }

  if (parsed.location && typeof parsed.location === 'string') {
    redirectArgs.location = parsed.location;
  }
  if (parsed.org_name && typeof parsed.org_name === 'string') {
    // eslint-disable-next-line @typescript-eslint/camelcase
    redirectArgs.org_name = parsed.org_name;
  }
  if (isSignup) {
    redirectArgs.signup = true;
  }
  const qs = QueryString.stringify(redirectArgs as any);
  const redirectURL = `${window.location.origin}/auth/callback?${qs}`;

  const segmentId = typeof parsed.tid === 'string' ? parsed.tid : '';
  if (segmentId) {
    analytics.alias(segmentId);
  }

  wa.authorize({
    connection: 'google-oauth2',
    responseType: 'token',
    redirectUri: redirectURL,
    prompt: 'login',
  });
};

export const auth0SignupRequest = () => auth0Request(true);
export const auth0LoginRequest = () => auth0Request(false);
