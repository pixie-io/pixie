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

import { ThemeProvider } from '@mui/material/styles';
import {
  render,
  fireEvent,
  screen,
  getByText,
  getByTestId,
} from '@testing-library/react';

import { DARK_THEME } from 'app/components';

import LiveViewShortcutsProvider, { getKeyMap, LiveShortcutsContext } from './shortcuts';

// eslint-disable-next-line react-memo/require-memo
const Consumer = () => {
  const ctx = React.useContext(LiveShortcutsContext);
  // The handlers actually want keyboard events, so the buttons attach to those.
  return (
    <>
      <button type='button' onKeyPress={(e?) => ctx['show-help'].handler(e?.nativeEvent)}>show-help</button>
      <button type='button' onKeyPress={(e?) => ctx['toggle-editor'].handler(e?.nativeEvent)}>toggle-editor</button>
      <button
        type='button'
        onKeyPress={(e?) => ctx['toggle-data-drawer'].handler(e?.nativeEvent)}
      >
        toggle-data-drawer
      </button>
      <button type='button' onKeyPress={(e?) => ctx.execute.handler(e?.nativeEvent)}>execute</button>
      {/* For testing shortcuts when the user is typing */}
      <input type='text' data-testid='shortcuts-input' />
    </>
  );
};

// eslint-disable-next-line react-memo/require-memo
const Tester = ({ inputHandlers }) => (
  <ThemeProvider theme={DARK_THEME}>
    <LiveViewShortcutsProvider handlers={inputHandlers}>
      <Consumer />
    </LiveViewShortcutsProvider>
  </ThemeProvider>
);

describe('Shortcut keys', () => {
  // The shortcut handler ignores combinations that would edit text in a currently-focused element (simple heuristic).
  // We're specifying the code and charCode due to https://github.com/testing-library/react-testing-library/issues/269
  const ignoredEvent = {
    ctrlKey: true, key: 'ArrowLeft', code: 37, charCode: 37,
  };
  const capturedEvent = {
    metaKey: true, key: 'Space', code: 32, charCode: 32,
  };

  it('defines a keymap', () => {
    const map = getKeyMap();
    expect(Object.keys(map).length).toBeGreaterThan(0);
    for (const mapping of Object.values(map)) {
      expect(mapping.description).not.toBeUndefined();
      expect(typeof mapping.sequence).not.toBeFalsy();
      expect(typeof mapping.displaySequence).not.toBeFalsy();
      expect(mapping.sequence && mapping.displaySequence).toBeTruthy();
    }
  });

  // A note: there is no test for user key presses activating shortcuts.
  // That happens upstream, in react-hotkeys, and should be tested there.
  describe('handlers', () => {
    const inputHandlers = {
      'toggle-editor': jest.fn(),
      'toggle-data-drawer': jest.fn(),
      execute: jest.fn(),
    };

    afterEach(() => {
      jest.resetAllMocks();
    });

    it('exposes programmatically callable handlers', () => {
      const { container } = render(<Tester inputHandlers={inputHandlers} />);
      for (const name of Object.keys(inputHandlers)) {
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        fireEvent.keyPress(getByText(container, name), capturedEvent);
        expect(inputHandlers[name]).toHaveBeenCalled();
      }
    });

    it('exposes a handler to show help about registered commands', () => {
      const { container } = render(<Tester inputHandlers={inputHandlers} />);

      expect(screen.queryByText('Available Shortcuts')).toBeNull();
      fireEvent.keyPress(getByText(container, 'show-help'), capturedEvent);
      expect(screen.queryByText('Available Shortcuts')).not.toBeNull();
    });

    it('ignores conflicting handlers when an editable element has focus', () => {
      const { container } = render(<Tester inputHandlers={inputHandlers} />);
      const input = getByTestId(container, 'shortcuts-input');
      for (const name of Object.keys(inputHandlers)) {
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        input.focus();
        expect(document.activeElement).not.toBe(document.body);
        const btn = getByText(container, name);
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        fireEvent.keyPress(btn, ignoredEvent);
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        fireEvent.keyPress(btn, capturedEvent);
        expect(inputHandlers[name]).toHaveBeenCalled();
      }

      expect(screen.queryByText('Available Shortcuts')).toBeNull();
      const helpBtn = getByText(container, 'show-help');
      fireEvent.keyPress(helpBtn, capturedEvent);
      expect(screen.queryByText('Available Shortcuts')).not.toBeNull();
    });
  });
});
