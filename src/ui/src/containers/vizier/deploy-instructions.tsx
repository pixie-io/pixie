import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {DOMAIN_NAME} from 'containers/constants';
import * as React from 'react';
import {Dropdown, DropdownButton} from 'react-bootstrap';

// @ts-ignore : TS does not like image files.
import * as docsImage from 'images/icons/docs.svg';
// @ts-ignore : TS does not like image files.
import * as downloadImage from 'images/icons/download.svg';
// @ts-ignore : TS does not like image files.
import * as emailImage from 'images/icons/email.svg';

import './deploy-instructions.scss';

interface DeployInstructionsProps {
  sitename: string;
  clusterID: string;
}

// TODO(michelle): Fill this out with the correct deploy methods.
const DEPLOY_METHODS = ['a', 'b'];

export const DeployInstructions = (props: DeployInstructionsProps) => {
  // TODO(michelle): Pull --use_version tag from backend.
  return (
    <div className='deploy-instructions'>
      <DialogBox width={760}>
        <div className='deploy-instructions--content'>
          <h3>Deployment Instructions</h3>
          <div className='deploy-instructions--instructions' style={{width: '100%'}}>
            <div className='deploy-instructions--notes'>
              Notes for Alpha Users:
             <ul>
                <li>The command uses the Pixie CLI to deploy Pixie on your cluster: </li>
                <li>Download the
                  <a href='/assets/downloads/pixie/linux_amd64/pixie'>{' Pixie CLI '}
                    <img src={downloadImage}/>
                  </a>
                    {' ('} <a href='/assets/downloads/pixie/linux_amd64/pixie.sha256'>{'SHA256 '}
                      <img src={downloadImage}/>
                    </a>{' )'}.
                </li>
                <li>The Pixie CLI is a Linux binary, so make sure you run it on a Linux machine.</li>
                <li>Include the path to the credentials file where it says {'/*<credentials file path>*/'}.</li>
                <li>Once Pixie is deployed, you must manually validate the SSL certs (see instructions
                  <a href='/docs/admin/authentication/'> here</a>).
                </li>
             </ul>
          </div>
          Copy and execute the command below in your K8s cluster's terminal:
            <CodeSnippet showCopy={true} language='bash'>
              {'./pixie deploy --cluster_id ' + props.clusterID +
                ' --use_version v0.1 \\ \n --credentials_file /*<credentials file path>*/'}
            </CodeSnippet>
          </div>
          <div className='deploy-instructions--footer' style={{width: '100%'}}>
            Need help with deploying your application?
            <div className='deploy-instructions--contact-info'>
              <div className='deploy-instructions--contact-info-line'>
                <img src={docsImage}/>
                  <a href={'https://' + DOMAIN_NAME + '/docs/getting-started/'}>Pixie Product Documentation</a>
              </div>
              <div className='deploy-instructions--contact-info-line'>
                <img src={emailImage}/><a href='mailto:cs@pixielabs.ai'> cs@pixielabs.ai</a>
              </div>
            </div>
          </div>
        </div>
      </DialogBox>
    </div>
  );
};
