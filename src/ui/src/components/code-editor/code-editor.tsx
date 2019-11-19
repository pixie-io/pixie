// Import code mirror dependencies.
import 'codemirror/lib/codemirror.css';
import 'codemirror/mode/python/python';
import 'codemirror/theme/monokai.css';
import './code-editor.scss';

import * as codemirror from 'codemirror';
import * as React from 'react';
import {Controlled as CodeMirror} from 'react-codemirror2';

interface CodeEditorProps {
  code?: string;
  onChange?: (code: string) => void;
  onSubmit?: (code: string) => void;
}

export class CodeEditor extends React.PureComponent<CodeEditorProps> {
  private editorInstance: codemirror.Editor | null = null;

  render() {
    const { code, onChange, onSubmit } = this.props;
    const options = {
      lineNumbers: true,
      mode: 'python',
      theme: 'monokai',
      extraKeys: {
        'Cmd-Enter': onSubmit,
        'Ctrl-Enter': onSubmit,
      },
    };
    return (
      <CodeMirror
        value={code}
        onBeforeChange={(editor, data, value) => {
          onChange(value);
        }}
        options={options}
        className='pl-code-editor'
        editorDidMount={(editor) => { this.editorInstance = editor; }}
      />
    );
  }
}
