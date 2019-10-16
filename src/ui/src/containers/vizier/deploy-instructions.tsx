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
            <div><span className='deploy-instructions--step'>Step 1:</span> Download the
                  <a href='/assets/downloads/pixie/linux_amd64/pixie'>{' Pixie CLI '}
                    <img src={downloadImage}/>
                  </a>
                    {' ('} <a href='/assets/downloads/pixie/linux_amd64/pixie.sha256'>{'SHA256 '}
                      <img src={downloadImage}/>
                    </a>{' )'}.
            </div>
            <br/>
            <div><span className='deploy-instructions--step'>Step 2:
              </span> Copy and execute the following commands in your K8s cluster terminal:
            </div>
            <p/>
            <CodeSnippet showCopy={true} language='bash'>
              {' chmod +x pixie'}
            </CodeSnippet>
            <CodeSnippet showCopy={true} language='bash'>
              {' ./pixie auth login --site="' + props.sitename + '"'}
            </CodeSnippet>
            <CodeSnippet showCopy={true} language='bash'>
              {' ./pixie deploy --cluster_id "' + props.clusterID + '"\n \\ --use_version v0.1.4'}
            </CodeSnippet>
            <br/>
            <br/>
            <div>Note: Pixie CLI is a linux binary. Make sure you run it on a Linux machine.</div>
          </div>
          <div className='deploy-instructions--footer' style={{width: '100%'}}>
            Need help with deploying your application?
            <div className='deploy-instructions--contact-info'>
              <div className='deploy-instructions--contact-info-line'>
                <img src={docsImage}/>
                  <a href={'/docs/getting-started/'}>Pixie Product Documentation</a>
              </div>
              <div className='deploy-instructions--contact-info-line'>
                <img src={emailImage}/><a href='mailto:cs@pixielabs.ai'>cs@pixielabs.ai</a>
              </div>
            </div>
          </div>
        </div>
      </DialogBox>
    </div>
  );
};
