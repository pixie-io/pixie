import * as React from 'react';

export interface AutocompleteContextProps {
  /** @default true */
  allowTyping?: boolean;
  /** @default true */
  requireCompletion?: boolean;
  /**
   * What to do when an associated input is "opened" - like first being created, or being revealed in a dropdown.
   * 'none': don't respond to the input being opened
   * 'focus': moves tab focus to the input
   * 'select': moves tab focus to the input, then selects its contents as if Ctrl/Cmd+A had been pressed.
   * 'clear': moves tab focus to the input, and sets its value to the empty string.
   * @default focus
   */
  onOpen?: 'none'|'focus'|'select'|'clear';
  hidden?: boolean;
  inputRef?: React.MutableRefObject<HTMLInputElement>;
}

export const AutocompleteContext = React.createContext<AutocompleteContextProps>({
  allowTyping: true,
  requireCompletion: true,
  onOpen: 'none',
  hidden: false,
});
