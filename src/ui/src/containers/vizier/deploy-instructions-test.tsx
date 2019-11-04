import { CodeSnippet } from 'components/code-snippet/code-snippet';
import { DialogBox } from 'components/dialog-box/dialog-box';
import { mount, shallow } from 'enzyme';
import * as React from 'react';
import { MockedProvider } from 'react-apollo/test-utils';

import {
    DeployInstructions, DeployInstructionsContent, GET_CLI_BINARY_INFO,
} from './deploy-instructions';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

describe('<DeployInstructions/> test', () => {
  it('should render content with correct props', async () => {
    window.URL.createObjectURL = jest.fn();

    const mocks = [
      {
        request: {
          query: GET_CLI_BINARY_INFO,
          variables: {},
        },
        result: {
          data: {
            linux: {
              url: 'http://pixie.ai/cliArtifact/linux',
              sha256: 'shaf0rl1nux',
            },
            macos: {
              url: 'http://pixie.ai/cliArtifact/macos',
              sha256: 'shaf0rmac0s',
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
    expect(wrapper.find(DeployInstructionsContent).get(0).props.cliBinaries).toEqual([
      {
        osName: 'Linux',
        url: 'http://pixie.ai/cliArtifact/linux',
        sha256: 'shaf0rl1nux',
      },
      {
        osName: 'macOS',
        url: 'http://pixie.ai/cliArtifact/macos',
        sha256: 'shaf0rmac0s',
      },
    ]);
  });
});

describe('<DeployInstructionsContent/> test', () => {
  it('should show correct content', () => {
    window.URL.createObjectURL = jest.fn((x) => 'bloburl');

    const wrapper = shallow(
      <DeployInstructionsContent
        sitename={'pixielabs'}
        clusterID={'test'}
        cliBinaries={
          [
            {
              osName: 'Linux',
              url: 'http://pixie.ai/cliArtifact/linux',
              sha256: 'shaf0rl1nux',
            },
            {
              osName: 'macOS',
              url: 'http://pixie.ai/cliArtifact/macos',
              sha256: 'shaf0rmac0s',
            },
          ]
        }
      />);

    expect(wrapper.find(CodeSnippet)).toHaveLength(4);
    expect(wrapper.find('.cli-download-link').get(0).props.href).toBe('http://pixie.ai/cliArtifact/linux');
    expect(wrapper.find('.cli-sha-link').get(0).props.href).toBe('bloburl');
    expect(wrapper.find('.cli-download-link').get(1).props.href).toBe('http://pixie.ai/cliArtifact/macos');
    expect(wrapper.find('.cli-sha-link').get(1).props.href).toBe('bloburl');
  });
});
