import * as React from 'react';

import { action } from '@storybook/addon-actions';
import { storiesOf } from '@storybook/react';

import { CodeEditor } from '../src/components/code-editor/code-editor';

class EditorWrapper extends React.Component<{}, { code: string }> {
  constructor(props) {
    super(props);
    this.state = {
      code: '',
    };
  }

  render() {
    const { code } = this.state;
    return (
      <CodeEditor
        onChange={(newCode) => { this.setState({ code: newCode }); }}
      />
    );
  }
}

storiesOf('CodeEditor', module)
  .add('Basic', () => (
    <CodeEditor
    />
  ), {
    info: { inline: true },
    notes: 'Code editor component. This component is a wrapper around react-codemirror2. The '
      + 'component is uncontrolled, meaning it does not manage state on it\'s own, subscribe to '
      + 'code changes using the onChange callback.',
  })
  .add('With Wrapper', () => <EditorWrapper />, {
    info: { inline: true },
    notes: 'Example with a wrapper component that binds the code changes to the state.',
  });
