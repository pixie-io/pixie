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
import { mount } from 'enzyme';
import { Modal } from '@material-ui/core';
import { act } from 'react-dom/test-utils';
import LiveViewShortcutsProvider, { getKeyMap, LiveShortcutsContext } from './shortcuts';

const Consumer = () => {
  const ctx = React.useContext(LiveShortcutsContext);
  // The handlers actually want keyboard events, so the buttons attach to those.
  return (
    <>
      <button type='button' onKeyPress={(e?) => ctx['show-help'].handler(e?.nativeEvent)}>show-help</button>
      <button type='button' onKeyPress={(e?) => ctx['pixie-command'].handler(e?.nativeEvent)}>pixie-command</button>
      <button type='button' onKeyPress={(e?) => ctx['toggle-editor'].handler(e?.nativeEvent)}>toggle-editor</button>
      <button
        type='button'
        onKeyPress={(e?) => ctx['toggle-data-drawer'].handler(e?.nativeEvent)}
      >
        toggle-data-drawer
      </button>
      <button type='button' onKeyPress={(e?) => ctx.execute.handler(e?.nativeEvent)}>execute</button>
      {/* For testing shortcuts when the user is typing */}
      <input type='text' />
    </>
  );
};

describe('Shortcut keys', () => {
  it('defines a keymap', () => {
    const map = getKeyMap();
    expect(Object.keys(map).length).toBeGreaterThan(0);
    for (const mapping of Object.values(map)) {
      expect(mapping.description).not.toBeFalsy();
      expect(typeof mapping.sequence).not.toBeFalsy();
      expect(typeof mapping.displaySequence).not.toBeFalsy();
      expect(mapping.description && mapping.sequence && mapping.displaySequence).toBeTruthy();
    }
  });

  // A note: there is no test for user key presses activating shortcuts.
  // That happens upstream, in react-hotkeys, and should be tested there.
  describe('handlers', () => {
    const inputHandlers = {
      'pixie-command': jest.fn(),
      'toggle-editor': jest.fn(),
      'toggle-data-drawer': jest.fn(),
      execute: jest.fn(),
    };

    afterEach(() => {
      jest.resetAllMocks();
    });

    it('exposes programmatically callable handlers', () => {
      const comp = mount(<LiveViewShortcutsProvider handlers={inputHandlers}><Consumer /></LiveViewShortcutsProvider>);
      for (const name of Object.keys(inputHandlers)) {
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        const btn = comp.find('button').filterWhere((w) => w.text() === name);
        btn.simulate('keypress');
        expect(inputHandlers[name]).toHaveBeenCalled();
      }
      comp.unmount();
    });

    it('exposes a handler to show help about registered commands', () => {
      const comp = mount(<LiveViewShortcutsProvider handlers={inputHandlers}><Consumer /></LiveViewShortcutsProvider>);

      expect(comp.find(Modal).props().open).toBe(false);
      const btn = comp.find('button').filterWhere((w) => w.text() === 'show-help');
      btn.simulate('keypress');
      expect(comp.find(Modal).props().open).toBe(true);

      comp.unmount();
    });

    it('ignores conflicting handlers when an editable element has focus', () => {
      // The method handling these shortcuts uses a simple heuristic to try to guess whether a key combination
      // would edit text in the focused content-editable element. If it would, then its global binding is ignored.
      // Otherwise - if it would not or if tab focus is not on an editable element - the binding is activated.
      const ignoredEvent = new KeyboardEvent('keypress', { ctrlKey: true, key: 'ArrowLeft' });
      const capturedEvent = new KeyboardEvent('keypress', { metaKey: true, key: 'MediaPlay' });

      // Using attachTo so that browser focus actually moves, see this issue:
      // https://github.com/enzymejs/enzyme/issues/2337#issuecomment-608984530
      // Also using a wrapper element to attach to, so that Enzyme does not warn about attaching directly to the body.
      const wrapper = document.createElement('div');
      document.body.appendChild(wrapper);
      const comp = mount(
        <LiveViewShortcutsProvider handlers={inputHandlers}><Consumer /></LiveViewShortcutsProvider>,
        { attachTo: wrapper },
      );
      const input = comp.find('input').at(0);
      for (const name of Object.keys(inputHandlers)) {
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        input.getDOMNode<HTMLInputElement>().focus();
        expect(document.activeElement).not.toBe(document.body);
        const btn = comp.find('button').filterWhere((w) => w.text() === name);
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        act(() => (btn.prop('onKeyPress') as (e: any) => void)({ nativeEvent: ignoredEvent }));
        expect(inputHandlers[name]).not.toHaveBeenCalled();
        act(() => (btn.prop('onKeyPress') as (e: any) => void)({ nativeEvent: capturedEvent }));
        expect(inputHandlers[name]).toHaveBeenCalled();
      }

      expect(comp.find(Modal).props().open).toBe(false);
      const helpBtn = comp.find('button').filterWhere((w) => w.text() === 'show-help');
      act(() => helpBtn.prop('onKeyPress')(undefined));
      expect(comp.find(Modal).props().open).toBe(false);
      comp.unmount();
    });
  });
});
