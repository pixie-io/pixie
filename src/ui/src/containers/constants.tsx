export const AUTH0_DOMAIN = '__CONFIG_AUTH0_DOMAIN__';
export const AUTH0_CLIENT_ID = '__CONFIG_AUTH0_CLIENT_ID__';
export const DOMAIN_NAME = '__CONFIG_DOMAIN_NAME__';
// If the value is not set, set it to empty. Otherwise, set it to the pre processed value.
export const SEGMENT_UI_WRITE_KEY = ('__SEGMENT_UI_WRITE_KEY__' === ('__' + 'SEGMENT_UI_WRITE_KEY__')) ?
       null : '__SEGMENT_UI_WRITE_KEY__';
