import clsx from 'clsx';
import * as _ from 'lodash';
import * as React from 'react';
import MonacoEditor from 'react-monaco-editor';

import { getKeyMap } from 'containers/live/shortcuts';

interface CodeEditorProps {
  onChange?: (code: string) => void;
  disabled?: boolean;
  className?: string;
  language?: string;
}

function removeKeybindings(editor, keys: string[]) {
    // Only way to disable default keybindings is through this private api according to:
    // https://github.com/microsoft/monaco-editor/issues/287.
  const bindings = editor._standaloneKeybindingService._getResolver()._defaultKeybindings;
  for (const bind of bindings) {
    for (const key of keys) {
      if (bind.keypressParts && bind.keypressParts[0].toLowerCase().includes(key.toLowerCase())) {
        editor._standaloneKeybindingService.addDynamicKeybinding(`-${bind.command}`);
      }
    }
  }
}

export class CodeEditor extends React.PureComponent<CodeEditorProps, any> {
  private editorRef;

  constructor(props) {
    super(props);
    this.state = {
      lineNumbers: true,
      extraEditorClassName: clsx('pl-code-editor', this.props.className),
      lineDecorationsWidth: 0,
      scrollBeyondLastColumn: 0,
      scrollBeyondLastLine: 0,
    };
    this.onChange = this.onChange.bind(this);
    this.onEditorMount = this.onEditorMount.bind(this);

    window.addEventListener('resize', () => {
      if (this.editorRef) {
        this.editorRef.layout();
      }
    });
  }

  changeEditorValue = (code) => {
    if (this.editorRef) {
      this.editorRef.setValue(code);
    }
  };

  onChange(code) {
    if (this.props.onChange) {
      this.props.onChange(code);
    }
  }

  onEditorMount(editor) {
    this.editorRef = editor;
    const shortcutKeys = _.flatMap(Object.values(getKeyMap()), (keybinding) => keybinding.sequence)
      .map((key) => key.toLowerCase().replace('control', 'ctrl'));
    removeKeybindings(editor, shortcutKeys);
  }

  render() {
    return (
      <MonacoEditor
        onChange={this.onChange}
        editorDidMount={this.onEditorMount}
        language={this.props.language ? this.props.language : 'python'}
        theme='vs-dark'
        options={this.state}
      />
    );
  }
}
