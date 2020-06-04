declare global {
  interface Window {
    __PIXIE_FLAGS__: {
      AUTH0_DOMAIN: string;
      AUTH0_CLIENT_ID: string;
      DOMAIN_NAME: string;
      SEGMENT_UI_WRITE_KEY: string;
    };
  }
}

export const AUTH0_DOMAIN = window.__PIXIE_FLAGS__.AUTH0_DOMAIN;
export const AUTH0_CLIENT_ID = window.__PIXIE_FLAGS__.AUTH0_CLIENT_ID;
export const DOMAIN_NAME = window.__PIXIE_FLAGS__.DOMAIN_NAME;
export const SEGMENT_UI_WRITE_KEY = window.__PIXIE_FLAGS__.SEGMENT_UI_WRITE_KEY;
