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

interface DeployInstructionsProps {
  sitename: string;
  clusterID: string;
}

interface CLIBinaryInfo {
  osName: string;
  url: string;
  sha256: string;
}

interface DeployInstructionsContentProps {
  sitename: string;
  clusterID: string;
  cliBinaries: CLIBinaryInfo[];
}

export const GET_CLI_BINARY_INFO = gql`
  query {
    linux: cliArtifact(artifactType: AT_LINUX_AMD64) {
      url
      sha256
    }
    macos: cliArtifact(artifactType: AT_DARWIN_AMD64) {
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
  return (<Query query={GET_CLI_BINARY_INFO}>
    {
      ({ loading, error, data }) => {
        if (loading) {
          return 'Loading...';
        }

        if (error) {
          return <p>Failed to get deployment instructions, please try again later.</p>;
        }

        const cliBinaries = [];

        if (data.linux) {
          cliBinaries.push({
            osName: 'Linux',
            url: data.linux.url,
            sha256: data.linux.sha256,
          });
        }

        if (data.macos) {
          cliBinaries.push({
            osName: 'macOS',
            url: data.macos.url,
            sha256: data.macos.sha256,
          });
        }

        return (<DeployInstructionsContent
          cliBinaries={cliBinaries}
          {...props}
        />);
      }
    }
  </Query>);
};

// DeployInstructionsContent contains the actual contents of the deploy instructions.
export class DeployInstructionsContent extends React.Component<DeployInstructionsContentProps, {}> {
  private createdURLs: string[] = [];

  componentWillUnmount() {
    for (const url of this.createdURLs) {
      window.URL.revokeObjectURL(url);
    }
  }

  render() {
    const { clusterID, sitename, cliBinaries } = this.props;

    return (<div className='deploy-instructions'>
      <DialogBox width={760}>
        <div className='deploy-instructions--content'>
          <h3>Deployment Instructions</h3>
          <div className='deploy-instructions--instructions' style={{ width: '100%' }}>
            <div><span className='deploy-instructions--step'>Step 1:</span>
              Download the Pixie CLI
              <ul>
                {
                  cliBinaries.map((info) => (
                    <li key={info.osName}>
                      <span>{info.osName}: </span>
                      <a className='cli-download-link' href={info.url}>{' Pixie CLI '}</a>
                      {' ('}
                      <a className='cli-sha-link' href={this.getBlobURL(info.sha256)} download='pixie.sha256'>
                        {'SHA256'}
                      </a>
                      {')'}.
                    </li>
                  ))
                }
              </ul>
            </div>
            <br />
            <div><span className='deploy-instructions--step'>Step 2:
              </span> Copy and execute the following commands in your K8s cluster terminal:
            </div>
            <p />
            <CodeSnippet showCopy={true} language='bash'>
              {'chmod +x px'}
            </CodeSnippet>
            <CodeSnippet showCopy={true} language='bash'>
              {'./px auth login --site="' + sitename + '"'}
            </CodeSnippet>
            <CodeSnippet showCopy={true} language='bash'>
              {'./px deploy --cluster_id "' + clusterID + '"'}
            </CodeSnippet>
            <br />
            <div><span className='deploy-instructions--step'>Step 3:
              </span> If your K8s cluster is on a separate network, create a proxy to Pixie's Vizier:
            </div>
            <p />
            <CodeSnippet showCopy={true} language='bash'>
              {'./px proxy'}
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

  private getBlobURL(data: string): string {
    const blob = new Blob([data], { type: 'octet/stream' });
    const url = window.URL.createObjectURL(blob);
    this.createdURLs.push(url);
    return url;
  }
}
