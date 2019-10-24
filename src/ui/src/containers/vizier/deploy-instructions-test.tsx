import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {mount, shallow} from 'enzyme';
import * as React from 'react';
import { MockedProvider } from 'react-apollo/test-utils';
import {DeployInstructions, DeployInstructionsContent, GET_LINUX_CLI_BINARY} from './deploy-instructions';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

describe('<DeployInstructions/> test', () => {
  it('should render content with correct props', async () => {
    window.URL.createObjectURL = jest.fn();

    const mocks = [
      {
        request: {
          query: GET_LINUX_CLI_BINARY,
          variables: {},
        },
        result: {
          data: {
            cliArtifact: {
              url: 'http://pixie.ai/cliArtifact',
              sha256: 'abcd',
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <DeployInstructions
          sitename={'pixielabs'}
          clusterID={'test'}
        />
      </MockedProvider>);
    await wait(0);
    wrapper.update();
    expect(wrapper.find(DeployInstructionsContent).get(0).props.sitename).toBe('pixielabs');
    expect(wrapper.find(DeployInstructionsContent).get(0).props.clusterID).toBe('test');
    expect(wrapper.find(DeployInstructionsContent).get(0).props.cliURL).toBe('http://pixie.ai/cliArtifact');
    expect(wrapper.find(DeployInstructionsContent).get(0).props.cliSHA256).toBe('abcd');
  });
});

describe('<DeployInstructionsContent/> test', () => {
  it('should show correct content', () => {
    window.URL.createObjectURL = jest.fn((x) => 'fake-blob');

    const wrapper = shallow(
      <DeployInstructionsContent
        sitename={'pixielabs'}
        clusterID={'test'}
        cliURL='http://pixie.ai/cliArtifact'
        cliSHA256='cli-sha256'
      />);

    expect(wrapper.find(CodeSnippet)).toHaveLength(3);
    expect(wrapper.find('#cli-download-link').get(0).props.href).toBe('http://pixie.ai/cliArtifact');
    expect(wrapper.find('#cli-sha-link').get(0).props.href).toBe('fake-blob');
  });
});
