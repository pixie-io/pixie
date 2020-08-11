declare global {
  interface Window {
    __PIXIE_FLAGS__: {
      AUTH0_DOMAIN: string;
      AUTH0_CLIENT_ID: string;
      DOMAIN_NAME: string;
      SEGMENT_UI_WRITE_KEY: string;
      LD_CLIENT_ID: string;
    };
  }
}

// eslint-disable-next-line no-underscore-dangle
export const { AUTH0_DOMAIN } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { AUTH0_CLIENT_ID } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { DOMAIN_NAME } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { SEGMENT_UI_WRITE_KEY } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { LD_CLIENT_ID } = window.__PIXIE_FLAGS__;
