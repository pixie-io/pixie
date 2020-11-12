/* eslint-disable no-underscore-dangle */
import clsx from 'clsx';
import * as _ from 'lodash';
import * as React from 'react';

import { getKeyMap } from 'containers/live/shortcuts';
import { Spinner } from 'pixie-components';
import { editor as MonacoEditorTypes } from 'monaco-editor';
import ICodeEditor = MonacoEditorTypes.ICodeEditor;

interface CodeEditorProps {
  code?: string;
  onChange?: (code: string) => void;
  /** Used to auto-focus the embedded Monaco editor when this component is shown */
  visible?: boolean;
  disabled?: boolean;
  className?: string;
  spinnerClass?: string;
  language?: string;
}

function removeKeybindings(editor, keys: string[]) {
  // Only way to disable default keybindings is through this private api according to:
  // https://github.com/microsoft/monaco-editor/issues/287.
  const bindings = editor._standaloneKeybindingService._getResolver()._defaultKeybindings;
  bindings.forEach((bind) => {
    keys.forEach((key) => {
      if (bind.keypressParts && bind.keypressParts[0].toLowerCase().includes(key.toLowerCase())) {
        editor._standaloneKeybindingService.addDynamicKeybinding(`-${bind.command}`);
      }
    });
  });
}

export class CodeEditor extends React.PureComponent<CodeEditorProps, any> {
  private editorRef: ICodeEditor;

  // Holder for code in the editor.
  private code;

  constructor(props) {
    super(props);
    this.state = {
      // eslint-disable-next-line react/no-unused-state
      extraEditorClassName: clsx(this.props.className),
      // eslint-disable-next-line react/no-unused-state
      lineDecorationsWidth: 0,
      // eslint-disable-next-line react/no-unused-state
      scrollBeyondLastColumn: 0,
      // eslint-disable-next-line react/no-unused-state
      scrollBeyondLastLine: 0,
      // eslint-disable-next-line react/no-unused-state
      fontFamily: 'Roboto Mono, monospace',
      // eslint-disable-next-line react/no-unused-state
      editorModule: null, // editorModule is the object containing MonacoEditor.
    };
    this.onChange = this.onChange.bind(this);
    this.onEditorMount = this.onEditorMount.bind(this);

    window.addEventListener('resize', () => {
      if (this.editorRef) {
        this.editorRef.layout();
      }
    });
  }

  componentDidMount() {
    return import(/* webpackPrefetch: true */ 'react-monaco-editor').then(({ default: MonacoEditor }) => {
      this.setState({ editorModule: MonacoEditor });
    });
  }

  componentDidUpdate(prevProps) {
    // TODO(nick) investigate why this causes the rest of the view to bounce.
    // if (this.props.visible && this.props.visible !== prevProps.visible && this.editorRef) {
    //   this.editorRef.focus();
    // }
  }

  onChange(code) {
    if (this.props.onChange) {
      this.props.onChange(code);
    }
  }

  onEditorMount(editor) {
    this.editorRef = editor;
    if (this.editorRef && this.props.visible) {
      this.editorRef.focus();
    }
    const shortcutKeys = _.flatMap(Object.values(getKeyMap()), (keybinding) => keybinding.sequence)
      .map((key) => key.toLowerCase().replace('control', 'ctrl'));
    removeKeybindings(editor, shortcutKeys);
    if (this.code) {
      this.changeEditorValue(this.code);
      this.onChange(this.code);
    }
  }

  getEditorValue = (): string => {
    if (this.editorRef) {
      return this.editorRef.getValue();
    }
    return '';
  };

  changeEditorValue = (code) => {
    if (!code) {
      return;
    }
    this.code = code;
    if (!this.editorRef) {
      return;
    }
    // Don't do anything if the code is the same.
    if (this.editorRef.getValue() === code) {
      return;
    }
    // Save state before setting the editor value.
    const pos = this.editorRef.getPosition();
    // Set the editor value.
    this.editorRef.setValue(code);
    // Return state to normal.
    this.editorRef.setPosition(pos);
  };

  render() {
    if (!this.state.editorModule) {
      return <div className={this.props.spinnerClass}><Spinner /></div>;
    }
    return (
      <this.state.editorModule
        onChange={this.onChange}
        editorDidMount={this.onEditorMount}
        language={this.props.language ? this.props.language : 'python'}
        theme='vs-dark'
        options={this.state}
      />
    );
  }
}
