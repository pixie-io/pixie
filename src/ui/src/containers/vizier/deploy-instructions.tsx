import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {DOMAIN_NAME} from 'containers/constants';
import gql from 'graphql-tag';
import * as React from 'react';
import {Query} from 'react-apollo';
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

interface DeployInstructionsContentProps {
  sitename: string;
  clusterID: string;
  cliURL: string;
  cliSHA256: string;
}

export const GET_LINUX_CLI_BINARY = gql`
  query {
    cliArtifact(artifactType: AT_LINUX_AMD64) {
      url
      sha256
    }
  }
`;

// TODO(michelle): Fill this out with the correct deploy methods.
const DEPLOY_METHODS = ['a', 'b'];

// DeployInstructions is a wrapper around DeployInstructionsContent, which queries
// for the CLI artifact information.
export const DeployInstructions = (props: DeployInstructionsProps) => {
  return (<Query query={GET_LINUX_CLI_BINARY}>
    {
      ({ loading, error, data }) => {
        let cliURL = '';
        let cliSHA256 = '';

        if (data.cliArtifact) {
          cliURL = data.cliArtifact.url;
          cliSHA256 = data.cliArtifact.sha256;
        }

        return (<DeployInstructionsContent
          cliURL={cliURL}
          cliSHA256={cliSHA256}
          {...props}
        />);
      }
    }
  </Query>);
};

// DeployInstructionsContent contains the actual contents of the deploy instructions.
export class DeployInstructionsContent extends React.Component<DeployInstructionsContentProps, {}> {
  private shaURL: string;

  componentWillUnmount() {
    if (this.shaURL) {
      window.URL.revokeObjectURL(this.shaURL);
      this.shaURL = '';
    }
  }

  render() {
    if (!this.shaURL && this.props.cliSHA256) {
      const blob = new Blob([this.props.cliSHA256], {type: 'octet/stream'});
      this.shaURL = window.URL.createObjectURL(blob);
    }

    return (<div className='deploy-instructions'>
      <DialogBox width={760}>
        <div className='deploy-instructions--content'>
          <h3>Deployment Instructions</h3>
          <div className='deploy-instructions--instructions' style={{width: '100%'}}>
            <div><span className='deploy-instructions--step'>Step 1:</span> Download the
                  <a id='cli-download-link' href={this.props.cliURL}>{' Pixie CLI '}
                    <img src={downloadImage}/>
                  </a>
                    {' ('} <a id='cli-sha-link' href={this.shaURL} download='pixie.sha256'>{'SHA256 '}
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
              {' ./pixie auth login --site="' + this.props.sitename + '"'}
            </CodeSnippet>
            <CodeSnippet showCopy={true} language='bash'>
              {' ./pixie deploy --cluster_id "' + this.props.clusterID + '"'}
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
    </div>);
  }
}
