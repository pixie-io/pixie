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

import * as React from 'react';

import { editor as MonacoEditorTypes, KeyCode } from 'monaco-editor';
// This must be `import type`, not `import`. The latter would defeat the lazy import of the actual component below.
import type { MonacoEditorProps } from 'react-monaco-editor';

import { COMMON_THEME } from 'app/components/mui-theme';
import { Spinner } from 'app/components/spinner/spinner';
import { buildClass } from 'app/utils/build-class';

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
  theme?: string;
  isReadOnly?: boolean;
  readOnlyReason?: string;
}

interface CodeEditorState {
  editorModule: React.ComponentType<MonacoEditorProps> | null;
  options: MonacoEditorTypes.IEditorOptions;
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

export class CodeEditor extends React.PureComponent<CodeEditorProps, CodeEditorState> {
  static readonly displayName = 'CodeEditor';

  private editorRef: MonacoEditorTypes.ICodeEditor;

  // Holder for code in the editor.
  private code: string;

  constructor(props: CodeEditorProps) {
    super(props);
    this.state = {
      editorModule: null, // editorModule is the object containing MonacoEditor.
      options: {
        extraEditorClassName: buildClass(this.props.className),
        lineDecorationsWidth: 0,
        scrollBeyondLastColumn: 3, // Prevents hiding text behind the minimap or the scrollbar. Expands the scroll area.
        scrollBeyondLastLine: false,
        fontFamily: COMMON_THEME.typography.monospace.fontFamily,
        readOnly: this.props.isReadOnly === true,
      },
    };
    this.onChange = this.onChange.bind(this);
    this.onEditorMount = this.onEditorMount.bind(this);

    window.addEventListener('resize', () => {
      if (this.editorRef) {
        this.editorRef.layout();
      }
    });
  }

  componentDidMount(): Promise<void> {
    return import(/* webpackPrefetch: true */ 'react-monaco-editor').then(
      ({ default: MonacoEditor }) => {
        this.setState({ editorModule: MonacoEditor });
      },
    );
  }

  onChange(code: string): void {
    if (this.props.onChange) {
      this.props.onChange(code);
    }
  }

  onEditorMount(editor: MonacoEditorTypes.ICodeEditor): void {
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
    this.initReadOnlyTooltip(editor);
  }

  getEditorValue = (): string => {
    if (this.editorRef) {
      return this.editorRef.getValue();
    }
    return '';
  };

  changeEditorValue = (code: string): void => {
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

  render(): React.ReactElement {
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
        theme={this.props.theme || 'vs-dark'}
        options={this.state.options}
      />
    );
  }

  private initReadOnlyTooltip(editor: MonacoEditorTypes.ICodeEditor) {
    // First, set up the tooltip that will appear if the user tries to edit a read-only editor
    const messageContributor = editor.getContribution('editor.contrib.messageController');
    (editor as any).onDidAttemptReadOnlyEdit(() => {
      (messageContributor as any).showMessage(
        this.props.readOnlyReason || 'Editor is read-only',
        editor.getPosition(),
      );
    });

    // Next, apply this workaround from https://github.com/microsoft/monaco-editor/issues/1873 so that Monaco notices
    // the typing event in the first place (which it normally doesn't due to the <textarea> being disabled)
    editor.onKeyDown((e) => {
      if (!this.props.isReadOnly) return;
      const modified = e.ctrlKey || e.altKey || e.metaKey;
      const probablyCharacter = (
        (e.keyCode >= KeyCode.KEY_0 && e.keyCode <= KeyCode.KEY_9) ||
        (e.keyCode >= KeyCode.NUMPAD_0 && e.keyCode <= KeyCode.NUMPAD_9) ||
        (e.keyCode >= KeyCode.KEY_A && e.keyCode <= KeyCode.KEY_Z) ||
        [
          KeyCode.Enter,
          KeyCode.Space,
          KeyCode.Delete,
          KeyCode.US_SEMICOLON,
          KeyCode.US_EQUAL,
          KeyCode.US_COMMA,
          KeyCode.US_MINUS,
          KeyCode.US_DOT,
          KeyCode.US_SLASH,
          KeyCode.US_BACKTICK,
          KeyCode.US_OPEN_SQUARE_BRACKET,
          KeyCode.US_BACKSLASH,
          KeyCode.US_CLOSE_SQUARE_BRACKET,
          KeyCode.US_QUOTE,
          KeyCode.OEM_8,
          KeyCode.OEM_102,
        ].includes(e.keyCode)
      );
      if (!modified && probablyCharacter) {
        editor.trigger('', 'type', { text: 'nothing' });
      }
    });
  }
}
