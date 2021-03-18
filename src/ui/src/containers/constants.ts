declare global {
  type OAuthProvider = 'hydra' | 'auth0';
  interface Window {
    __PIXIE_FLAGS__: {
      OAUTH_PROVIDER: OAuthProvider;
      AUTH_URI: string;
      AUTH_CLIENT_ID: string;
      DOMAIN_NAME: string;
      SEGMENT_UI_WRITE_KEY: string;
      LD_CLIENT_ID: string;
      ANALYTICS_ENABLED: boolean;
      ANNOUNCEMENT_ENABLED: boolean;
      ANNOUNCE_WIDGET_URL: string;
      CONTACT_ENABLED: boolean;
    };
  }
}

// eslint-disable-next-line no-underscore-dangle
export const { OAUTH_PROVIDER } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { AUTH_URI } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { AUTH_CLIENT_ID } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { DOMAIN_NAME } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { SEGMENT_UI_WRITE_KEY } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { LD_CLIENT_ID } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { ANALYTICS_ENABLED } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { ANNOUNCEMENT_ENABLED } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { ANNOUNCE_WIDGET_URL } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { CONTACT_ENABLED } = window.__PIXIE_FLAGS__;
