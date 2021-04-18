/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { buildClass } from 'utils/build-class';
import * as React from 'react';

import { Spinner } from 'components/spinner/spinner';
import { editor as MonacoEditorTypes } from 'monaco-editor';

interface CodeEditorProps {
  code?: string;
  onChange?: (code: string) => void;
  /** Used to auto-focus the embedded Monaco editor when this component is shown */
  visible?: boolean;
  disabled?: boolean;
  className?: string;
  spinnerClass?: string;
  language?: string;
  shortcutKeys: string[];
}

function removeKeybindings(editor, keys: string[]) {
  /* eslint-disable no-underscore-dangle */
  // Only way to disable default keybindings is through this private api according to:
  // https://github.com/microsoft/monaco-editor/issues/287.
  const bindings = editor._standaloneKeybindingService._getResolver()
    ._defaultKeybindings;
  bindings.forEach((bind) => {
    keys.forEach((key) => {
      if (
        bind.keypressParts
        && bind.keypressParts[0].toLowerCase().includes(key.toLowerCase())
      ) {
        editor._standaloneKeybindingService.addDynamicKeybinding(
          `-${bind.command}`,
        );
      }
    });
  });
  /* eslint-enable no-underscore-dangle */
}

export class CodeEditor extends React.PureComponent<CodeEditorProps, any> {
  private editorRef: MonacoEditorTypes.ICodeEditor;

  // Holder for code in the editor.
  private code;

  constructor(props) {
    super(props);
    this.state = {
      // eslint-disable-next-line react/no-unused-state
      extraEditorClassName: buildClass(this.props.className),
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
    return import(/* webpackPrefetch: true */ 'react-monaco-editor').then(
      ({ default: MonacoEditor }) => {
        this.setState({ editorModule: MonacoEditor });
      },
    );
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
    const shortcutKeys = this.props.shortcutKeys.map((key) => key.toLowerCase().replace('control', 'ctrl'),
    );
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
      return (
        <div className={this.props.spinnerClass}>
          <Spinner />
        </div>
      );
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
