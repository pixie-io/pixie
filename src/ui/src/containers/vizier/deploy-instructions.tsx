import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {DOMAIN_NAME} from 'containers/constants';
import * as React from 'react';
import {Dropdown, DropdownButton} from 'react-bootstrap';

// @ts-ignore : TS does not like image files.
import * as emailImage from 'images/icons/email.svg';
// @ts-ignore : TS does not like image files.
import * as phoneImage from 'images/icons/phone.svg';

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
          <h3>Deploy pixie agent</h3>
          <div className='deploy-instructions--subheader'>{props.sitename}</div>
          <div className='deploy-instructions--instructions' style={{width: '100%'}}>
            <label htmlFor='deploy'>Deploy Method</label>
            <DropdownButton
              id='deploy-method-dropdown'
              title='Select the deploy method'
            >
              {
                DEPLOY_METHODS.map((method, idx) => {
                  return <Dropdown.Item
                    key={idx}
                    eventKey={idx}
                  >
                    {method}
                  </Dropdown.Item>;
                })
              }
            </DropdownButton>
            <CodeSnippet showCopy={true} language='bash'>
              {'./pixie deploy --cluster_id ' + props.clusterID +
                ' --use_version v0.1 --credentials_file <creds file path here>'}
            </CodeSnippet>
          </div>
          <div className='deploy-instructions--footer' style={{width: '100%'}}>
            Need help with deploying your application? Read through the documentation
            <a
              href={'https://' + DOMAIN_NAME + '/docs'}>here
            </a>.
            <br />
            If you have any additional questions, please contact us.
            <div className='deploy-instructions--contact-info'>
              <div className='deploy-instructions--contact-info-line'>
                <img src={phoneImage}/> (xxx)xxx-xxxx
              </div>
              <div className='deploy-instructions--contact-info-line'>
                <img src={emailImage}/> cs@pixielabs.ai
              </div>
            </div>
          </div>
        </div>
      </DialogBox>
    </div>
  );
};
