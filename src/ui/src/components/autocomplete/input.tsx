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

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';
import { buildClass } from 'app/utils/build-class';

import { Key } from './key';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    ...theme.typography.h6,
    position: 'relative',
    cursor: 'text',
    padding: `${theme.spacing(1.25)} ${theme.spacing(3)}`,
    fontWeight: theme.typography.fontWeightLight,
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'baseline',
  },
  inputWrapper: {
    position: 'relative',
    minWidth: '0.1px',
  },
  inputElem: {
    // Match the root surrounding it
    color: 'inherit',
    background: 'inherit',
    border: 'none',
    fontSize: 'inherit',
    fontWeight: 'inherit',
    lineHeight: 'inherit',
    position: 'absolute',
    top: '0',
    left: '0',
    padding: '0',
    // <input /> is a replaced element, and as such is frustratingly disobedient with CSS. Its width calculation is
    // off slightly; oversizing it allows it to fit its whole contents when growing to, well, fit its contents.
    // The hint still lines up because its position is based on the adjacent dummy, which sizes correctly on its own.
    // https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#CSS
    // https://developer.mozilla.org/en-US/docs/Web/CSS/Replaced_element
    width: '120%',
    minWidth: '1px', // Ensure the blinking cursor is visible when input is focused.
  },
  // Since HTMLInputElement does not obey normal width calculations, we position it atop an invisible span that does.
  dummy: {
    color: 'transparent',
    pointerEvents: 'none',
    paddingRight: '0.01px', // Guarantees that inputWrapper will have a nonzero width, so the input knows where to be.
  },
  hint: {
    opacity: 0.2,
    pointerEvents: 'none',
    userSelect: 'none',
  },
  textArea: {
    flex: 1,
  },
  prefix: {
    paddingRight: theme.spacing(2),
  },
}), { name: 'Input' });

interface InputProps {
  onChange: (val: string) => void;
  onKey: (key: Key) => void;
  suggestion?: string;
  placeholder?: string;
  prefix?: React.ReactNode;
  className?: string;
  value: string;
  customRef?: React.MutableRefObject<HTMLInputElement>;
}

// eslint-disable-next-line react-memo/require-memo
export const Input: React.FC<InputProps> = ({
  onChange,
  onKey,
  suggestion,
  className,
  placeholder = '',
  prefix = null,
  value,
  customRef,
}) => {
  const classes = useStyles();
  const { onOpen, hidden } = React.useContext(AutocompleteContext);
  const dummyElement = React.useRef<HTMLSpanElement>(null);
  const defaultRef = React.useRef<HTMLInputElement>(null);
  const inputRef = customRef || defaultRef;

  React.useEffect(() => {
    if (hidden || onOpen === 'none' || !inputRef.current) return;

    if (onOpen === 'select') inputRef.current.setSelectionRange(0, inputRef.current.value.length);
    if (onOpen === 'clear') onChange('');
    inputRef.current.focus();
  }, [onChange, onOpen, inputRef, hidden]);

  const handleChange = React.useCallback(
    (e) => {
      const val = e.target.value;
      onChange(val);
    },
    [onChange],
  );

  const handleKey = React.useCallback(
    (e) => {
      switch (e.key) {
        case 'Tab':
          e.preventDefault();
          onKey('TAB');
          break;
        case 'Enter':
          onKey('ENTER');
          break;
        case 'ArrowUp':
          onKey('UP');
          break;
        case 'ArrowDown':
          onKey('DOWN');
          break;
        default:
        // noop
      }
    },
    [onKey],
  );

  const focusInput = React.useCallback(() => {
    inputRef.current?.focus();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Focus the input element whenever the suggestion changes.
  React.useEffect(() => {
    inputRef.current?.focus();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [suggestion]);

  const hint = React.useMemo(() => {
    if (!value) {
      return placeholder;
    }
    return suggestion && suggestion.startsWith(value)
      ? suggestion.slice(value.length)
      : '';
  }, [value, suggestion, placeholder]);

  return (
    <div className={buildClass(classes.root, className)} onClick={focusInput}>
      <span className={classes.inputWrapper}>
        <span className={classes.dummy} ref={dummyElement}>
          {value}
        </span>
        <input
          type='text'
          className={classes.inputElem}
          ref={inputRef}
          value={value}
          onChange={handleChange}
          onKeyDown={handleKey}
          size={0} // Ensures that CSS controls the width of this element
        />
      </span>
      {prefix ? <div className={classes.prefix}>{prefix}</div> : null}
      <span className={classes.hint} tabIndex={-1}>
        {hint}
      </span>
    </div>
  );
};
Input.displayName = 'Input';
