// Import code mirror dependencies.
import 'codemirror/lib/codemirror.css';
import 'codemirror/mode/python/python';
import 'codemirror/theme/monokai.css';
import './code-editor.scss';

import clsx from 'clsx';
import * as codemirror from 'codemirror';
import * as React from 'react';
import {Controlled as CodeMirror} from 'react-codemirror2';

interface CodeEditorProps {
  code?: string;
  onChange?: (code: string) => void;
  onSubmit?: () => void;
  disabled?: boolean;
  className?: string;
}

export class CodeEditor extends React.PureComponent<CodeEditorProps, any> {
  private editorRef: codemirror.Editor;

  constructor(props) {
    super(props);
    const submitFunc = (editor) => {
      const { disabled, onSubmit } = this.props;
      if (disabled || !onSubmit) {
        return;
      }
      onSubmit();

      // For some reason after submitting code, you cannot type in the editor
      // unless the the textarea is refocused.
      const activeElem = document.activeElement as HTMLElement;
      if (activeElem && activeElem.blur) {
        activeElem.blur();
        editor.focus();
      }
    };
    this.state = {
      lineNumbers: true,
      mode: 'python',
      theme: 'monokai',
      extraKeys: {
        'Cmd-Enter': submitFunc,
        'Ctrl-Enter': submitFunc,
      },
    };
    this.onChange = this.onChange.bind(this);
    this.onEditorMount = this.onEditorMount.bind(this);
  }

  onChange(editor, data, code) {
    if (this.props.onChange) {
      this.props.onChange(code);
    }
  }

  onEditorMount(editor: codemirror.Editor) {
    this.editorRef = editor;
    editor.setSize('100%', '100%');
    editor.refresh();
  }

  render() {
    return (
      <CodeMirror
        value={this.props.code}
        onBeforeChange={this.onChange}
        options={this.state}
        className={clsx('pl-code-editor', this.props.className)}
        editorDidMount={this.onEditorMount}
      />
    );
  }
}
