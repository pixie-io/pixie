import './deploy-instructions.scss';

import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {DOMAIN_NAME} from 'containers/constants';
import gql from 'graphql-tag';
// @ts-ignore : TS does not like image files.
import * as docsImage from 'images/icons/docs.svg';
// @ts-ignore : TS does not like image files.
import * as downloadImage from 'images/icons/download.svg';
// @ts-ignore : TS does not like image files.
import * as emailImage from 'images/icons/email.svg';
import * as React from 'react';
import {Query} from 'react-apollo';
import {Dropdown, DropdownButton} from 'react-bootstrap';

// DeployInstructionsContent contains the contents of the deploy instructions.
export const DeployInstructions = () => {
  return (<div className='deploy-instructions'>
    <DialogBox width={760}>
      <div className='deploy-instructions--content'>
        <h3>Deployment Instructions</h3>
        <div className='deploy-instructions--instructions' style={{ width: '100%' }}>
          <div><span className='deploy-instructions--step'>Step 1:
            </span> Copy and execute the following commands in your K8s cluster terminal:
          </div>
          <p />
          <CodeSnippet showCopy={true} language='bash'>
            {'curl -fsSL ' + window.location.href + 'install.sh | bash'}
          </CodeSnippet>
          <CodeSnippet showCopy={true} language='bash'>
            {'px deploy'}
          </CodeSnippet>
          <br />
          <div><span className='deploy-instructions--step'>Step 2:
            </span> If your K8s cluster is on a separate network, create a proxy to Pixie's Vizier:
          </div>
          <p />
          <CodeSnippet showCopy={true} language='bash'>
            {'px proxy'}
          </CodeSnippet>
        </div>
        <div className='deploy-instructions--footer' style={{ width: '100%' }}>
          Need help with deploying your application?
          <div className='deploy-instructions--contact-info'>
            <div className='deploy-instructions--contact-info-line'>
              <img src={docsImage} />
              <a href={'/docs/getting-started/'}>Pixie Product Documentation</a>
            </div>
            <div className='deploy-instructions--contact-info-line'>
              <img src={emailImage} /><a href='mailto:cs@pixielabs.ai'>cs@pixielabs.ai</a>
            </div>
          </div>
        </div>
      </div>
    </DialogBox>
  </div>);
}
