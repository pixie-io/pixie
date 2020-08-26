import Axios from 'axios';
import { isStaging } from 'utils/env';

const PROD_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json';
const OSS_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-oss.json';
const STAGING_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-staging-core.json';

export interface Script {
  id: string;
  title: string;
  code: string;
  vis?: string;
  description?: string;
  hidden?: boolean;
}

interface ScriptJSON {
  ShortDoc: string;
  pxl: string;
  vis: string;
  LongDoc: string;
  hidden: boolean;
  orgName: string;
}

export function GetPxScripts(orgName: string): Promise<Script[]> {
  const bundlePath = localStorage.getItem('px-custom-core-bundle-path')
    || (isStaging() ? STAGING_SCRIPTS : PROD_SCRIPTS);
  const ossBundlePath = localStorage.getItem('px-custom-oss-bundle-path') || OSS_SCRIPTS;
  const fetchPromises = [Axios({ method: 'get', url: bundlePath }), Axios({ method: 'get', url: ossBundlePath })];
  return Promise.all(fetchPromises).then((response) => {
    const scripts = [];
    response.forEach((resp) => {
      Object.entries(resp.data.scripts as ScriptJSON[]).forEach(([id, s]) => {
        if (s.orgName && orgName !== s.orgName) {
          return;
        }
        scripts.push({
          id,
          title: s.ShortDoc,
          code: s.pxl,
          vis: s.vis,
          description: s.LongDoc,
          hidden: s.hidden,
        });
      });
    });
    return scripts;
  });
}
