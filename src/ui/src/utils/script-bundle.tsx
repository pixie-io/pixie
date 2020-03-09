import Axios from 'axios';
import {isProd} from 'utils/env';

const PROD_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json';
const STAGING_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-staging.json';

export interface Script {
  id?: string;
  title: string;
  code: string;
  vis?: string;
  placement?: string;
}

export const GetPxScripts = (callback) => {
  Axios({
    method: 'get',
    url: isProd() ? PROD_SCRIPTS : STAGING_SCRIPTS,
  }).then((response) => {
    const scripts = Object.keys(response.data.scripts).map((k) => {
      const s = response.data.scripts[k];
      return {
        title: k,
        code: s.pxl,
        vis: s.vis,
        placement: s.placement,
      };
    });
    callback(scripts);
  });
};
