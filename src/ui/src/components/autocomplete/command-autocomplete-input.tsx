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

import { buildClass } from 'app/utils/build-class';
import * as React from 'react';

import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

import { Key } from './key';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    ...theme.typography.h6,
    cursor: 'text',
    padding: theme.spacing(2.5),
    fontWeight: theme.typography.fontWeightLight,
    display: 'flex',
    flexDirection: 'row',
  },
  inputElem: {
    flex: 1,
    ...theme.typography.h6,
    position: 'absolute',
    opacity: 0,
    width: '100%',
    border: 0,
    fontWeight: theme.typography.fontWeightLight,
  },
  fixedInput: {
    width: '100%',
    overflow: 'hidden',
  },
  caret: {
    display: 'inline-block',
    width: 0,
    height: 'auto',
    borderRight: '1px solid white',
    visibility: 'hidden',
    '&.visible': {
      visibility: 'visible',
    },
    position: 'absolute',
  },
  inputValue: {
    flex: 1,
    whiteSpace: 'nowrap',
    display: 'inline-block',
    position: 'relative',
    height: '100%',
  },
  prefix: {
    paddingRight: theme.spacing(2),
    display: 'flex',
    alignSelf: 'center',
  },
  inputKey: {
    color: `${theme.palette.success.main}`,
  },
  hint: {
    opacity: 0.2,
    position: 'absolute',
  },
  valid: {
    border: theme.palette.success.main,
    borderStyle: 'solid',
  },
  invalid: {
    border: theme.palette.background.two,
    borderStyle: 'solid',
  },
}));

const BLINK_INTERVAL = 500; // 500ms = .5s

const Caret: React.FC<{ active: boolean }> = ({ active }) => {
  const classes = useStyles();
  const [visible, setVisible] = React.useState(true);
  React.useEffect(() => {
    if (!active) {
      return;
    }
    setInterval(() => {
      setVisible((show) => !show);
    }, BLINK_INTERVAL);
  }, [active]);
  return (
    <div className={buildClass(classes.caret, active && visible && 'visible')}>
      &nbsp;
    </div>
  );
};

export interface AutocompleteField {
  type: 'key' | 'value'; // The type signifies what color the text should be.
  value: string;
}

interface AutocompleteInputProps {
  onChange: (val: string, cursor: number) => void;
  onKey: (key: Key) => void;
  className?: string;
  value: Array<AutocompleteField>; // The text to show in the input box, marked by type.
  cursorPos: number; // Where the cursor position should be.
  prefix?: React.ReactNode; // An image to display to the left of the input box, if any.
  suffix?: React.ReactNode; // Something to display on the right of the input box, such as a hotkey hint.
  // Clicking around in the input box may result in a cursor change. This allows AutocompleteInput to notify the
  // parentClass of this change.
  setCursor: (val: number) => void;
  placeholder?: string; // The text to show if the input box is empty.
  isValid: boolean;
}

export const CommandAutocompleteInput: React.FC<AutocompleteInputProps> = ({
  onChange,
  onKey,
  className,
  cursorPos,
  setCursor,
  prefix = null,
  suffix = null,
  placeholder = '',
  value,
  isValid,
}) => {
  const classes = useStyles();
  const [focused, setFocused] = React.useState<boolean>(true);
  const inputRef = React.useRef<HTMLInputElement>(null);

  const handleChange = React.useCallback(
    (e) => {
      onChange(e.target.value, e.target.selectionStart);
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
        case 'ArrowRight':
          onKey('RIGHT');
          break;
        case 'ArrowLeft':
          onKey('LEFT');
          break;
        case 'Backspace':
          onKey('BACKSPACE');
          e.preventDefault();
          break;
        default:
          e.target.setSelectionRange(cursorPos, cursorPos);
      }
    },
    [onKey, cursorPos],
  );

  const handleFocus = React.useCallback(() => {
    setFocused(true);
  }, []);

  const handleBlur = React.useCallback(() => {
    setFocused(false);
  }, []);

  const focusInput = React.useCallback(() => {
    // The user has clicked a new position in the input box. We must
    // update the cursor position accordingly.
    setCursor(inputRef.current.selectionStart);
    inputRef.current?.focus();
  }, [setCursor]);

  // Focus the input element whenever the suggestion changes.
  React.useEffect(() => {
    inputRef.current?.focus();
  }, [value]);

  // Each field in the input box is a different span so that we can control color.
  const fieldSpans = [];
  let displayText = '';
  value.forEach((v, index) => {
    const valueClassName = v.type === 'value' ? '' : classes.inputKey;
    if (
      cursorPos > displayText.length
      && cursorPos <= displayText.length + v.value.length
    ) {
      // The cursor should be displayed in this field.
      // We need to split this field into two spans so we can add the cursor in between.
      const fieldCursor = cursorPos - displayText.length;
      const fspan1 = v.value.substring(0, fieldCursor);
      const fspan2 = v.value.substring(fieldCursor);

      fieldSpans.push(
        <span key={`${index}-1`} className={valueClassName}>
          {fspan1 === ' ' ? '\u00A0' : fspan1}
        </span>,
      );
      fieldSpans.push(<Caret key={`${index}-caret`} active={focused} />);
      fieldSpans.push(
        <span key={`${index}-2`} className={valueClassName}>
          {fspan2 === ' ' ? '\u00A0' : fspan2}
        </span>,
      );
    } else {
      fieldSpans.push(
        <span key={index} className={valueClassName}>
          {v.value === ' ' ? '\u00A0' : v.value}
        </span>,
      );
    }
    displayText += v.value;
  });

  return (
    <div
      className={buildClass(
        classes.root,
        className,
        isValid ? classes.valid : classes.invalid,
      )}
      onClick={focusInput}
    >
      {prefix && <div className={classes.prefix}>{prefix}</div>}
      <div className={classes.fixedInput}>
        <div className={classes.inputValue}>
          <input
            className={classes.inputElem}
            ref={inputRef}
            value={displayText}
            onChange={handleChange}
            onFocus={handleFocus}
            onBlur={handleBlur}
            onKeyDown={handleKey}
          />
          {fieldSpans}
        </div>
        <span className={classes.hint} tabIndex={-1}>
          {displayText.length === 0 ? placeholder : ''}
        </span>
      </div>
      {suffix && <div>{suffix}</div>}
    </div>
  );
};
