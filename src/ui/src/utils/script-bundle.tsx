import Axios from 'axios';
import { isStaging } from 'utils/env';

const PROD_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json';
const STAGING_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-staging.json';

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
  const bundlePath = localStorage.getItem('px-custom-bundle-path')
    || (isStaging() ? STAGING_SCRIPTS : PROD_SCRIPTS);
  return Axios({
    method: 'get',
    url: bundlePath,
  }).then((response) => {
    const scripts = [];
    Object.entries(response.data.scripts as ScriptJSON[]).forEach(([id, s]) => {
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
    return scripts;
  });
}
