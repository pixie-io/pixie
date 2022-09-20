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

import {
  Autocomplete,
  Paper,
  PaperProps,
  Popper,
  PopperProps,
  TextField,
} from '@mui/material';
import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { buildClass, PixieCommandIcon } from 'app/components';

import { CommandPaletteSuffix } from './command-palette-affixes';
import { CommandPaletteContext } from './command-palette-context';
import { CommandInputToken } from './command-tokens';
import { CommandCompletion } from './providers/command-provider';

const useFieldStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    position: 'relative',
  },
  overlay: {
    position: 'absolute',
    top: 0,
    left: theme.spacing(1.5 + 3), // left padding + startAdornment total width
    lineHeight: theme.spacing(5),
    maxWidth: `calc(100% - ${theme.spacing(3)})`,
    pointerEvents: 'none',
    overflow: 'hidden',
  },
  input: {
    background: 'transparent',

    // TODO(nick): Only apply this border-radius if the autocomplete is open and visible
    '.Mui-focused &': {
      borderBottomLeftRadius: 0,
      borderBottomRightRadius: 0,
    },

    // The overlay colors text and may replace symbols. Don't show the plain text, but do show the caret.
    '& input': {
      color: 'transparent',
      caretColor: theme.palette.text.primary,
    },
  },
  paper: {
    borderTopLeftRadius: 0,
    borderTopRightRadius: 0,
    // TODO(nick): Update this border to match that of the input (multiple focus+active states)
    border: `${theme.spacing(0.25)} ${theme.palette.primary.main} solid`,
    borderTopWidth: 0,

    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'stretch',
    alignItems: 'stretch',

    '& > ul': {
      flex: '6 0 60%',
    },
  },
  paperDetails: {
    flex: '4 0 40%',
    display: 'flex',
    flexFlow: 'column nowrap',
    borderLeft: theme.palette.border.focused,
    padding: theme.spacing(1),
    height: theme.spacing(30),
    overflowY: 'auto',
  },
  centerDetails: {
    margin: 'auto',
  },
  popper: { /* Keep for reference */ },
}), { name: 'CommandTextField' });

const CommandTextFieldPaper = React.memo<PaperProps>(({ children, ...props }) => {
  const classes = useFieldStyles();
  const { completions, highlightedCompletion } = React.useContext(CommandPaletteContext);
  return (
    <Paper {...props} className={buildClass(classes.paper, props.className)}>
      {children}
      <div className={classes.paperDetails}>
        {(completions.length && highlightedCompletion?.description) || (
          <div className={classes.centerDetails}>
            {completions.length ? 'Select an option on the left.' : 'No results.'}
          </div>
        )}
      </div>
    </Paper>
  );
});
CommandTextFieldPaper.displayName = 'CommandTextFieldPaper';

const CommandTextFieldPopper = React.memo<PopperProps>(({ children, className, style, ...props }) => {
  const theme = useTheme();
  const classes = useFieldStyles();

  // Overlap the bottom border of the input
  const popperOptions = React.useMemo(() => ({
    modifiers: [{
      name: 'offset',
      options: { offset: [0, parseInt(theme.spacing(-0.25))] },
    }],
  }), [theme]);

  // TODO(nick): Left side of the popper is 0.5px too far to the right. Tweaking width or offset here both overshoot?
  const wider = React.useMemo(() => ({
    ...style,
    // width: (style.width as number) + 0.25,
  }), [style]);

  return (
    <Popper
      {...props}
      className={buildClass(classes.popper, className)}
      popperOptions={popperOptions}
      style={wider}
    >
      {children}
    </Popper>
  );
});
CommandTextFieldPopper.displayName = 'CommandTextFieldPopper';

export interface CommandTextFieldProps {
  text: string;
}

export const CommandTextField = React.memo<CommandTextFieldProps>(({
  text,
}) => {
  const classes = useFieldStyles();

  const {
    setOpen,
    inputValue,
    setInputValue,
    selection,
    setSelection,
    tokens,
    completions,
    setHighlightedCompletion,
    activateCompletion,
  } = React.useContext(CommandPaletteContext);

  React.useEffect(() => setInputValue(text), [text, setInputValue]);

  const [inputEl, setInputEl] = React.useState<HTMLInputElement>(null);
  const setInputRef = React.useCallback((el: HTMLInputElement) => setInputEl(el), []);

  // If a completion moves the selection, wait for the value to update and then move the real caret.
  React.useEffect(() => {
    if (!inputEl || inputEl.value !== inputValue) return;
    if (selection[0] !== inputEl.selectionStart || selection[1] !== inputEl.selectionEnd) {
      inputEl.setSelectionRange(selection[0], selection[1]);
    }
  }, [inputEl, inputValue, selection]);

  // If the user moves the caret themselves, update the internal representation.
  const onTextSelect = React.useCallback((event: React.SyntheticEvent) => {
    const t = event.target as HTMLInputElement;
    setSelection((prev) => {
      if (prev[0] !== t.selectionStart || prev[1] !== t.selectionEnd) {
        return [t.selectionStart, t.selectionEnd];
      } else return prev;
    });
  }, [setSelection]);

  /* eslint-disable react-memo/require-usememo */
  return (
    <div className={classes.root}>
      <Autocomplete
        size='small'
        freeSolo
        openOnFocus
        fullWidth
        disableClearable // The button this would create clears the Autocomplete selection, not the input (misleading).
        disableCloseOnSelect
        loadingText='Loading...'
        options={completions}
        getOptionLabel={(o: CommandCompletion) => o?.key ?? ''}
        noOptionsText='No results'
        onFocus={() => { setOpen(true); }}
        onBlur={() => { setOpen(false); }}
        filterOptions={(opts) => opts} // They're already filtered, but when renderOption exists, so must this.
        renderOption={(optProps, option: CommandCompletion) => <li {...optProps}>{option.label}</li>}
        onChange={(_, option: CommandCompletion) => { // Takes event, option, reason, details (in case we need them)
          activateCompletion(option);
        }}
        onHighlightChange={(_, option: CommandCompletion) => {
          setHighlightedCompletion(option);
        }}
        renderInput={(params) => (
          <TextField
            {...params}
            inputRef={setInputRef}
            onSelect={onTextSelect}
            InputProps={{
              ...params.InputProps,
              type: 'search',
              className: buildClass(params.InputProps.className, classes.input),
              startAdornment: <PixieCommandIcon />,
              endAdornment: <CommandPaletteSuffix />,
            }}
          />
        )}
        inputValue={inputValue}
        onInputChange={(e) => {
          const t = e?.target as HTMLInputElement;
          if (typeof t?.value === 'string') {
            setInputValue(t.value);
            setSelection([t.selectionStart, t.selectionEnd]);
          }
        }}
        PaperComponent={CommandTextFieldPaper}
        PopperComponent={CommandTextFieldPopper}
      />
      <div className={classes.overlay} aria-hidden>
        {tokens.map((token, i) => (
          <CommandInputToken key={i} token={token} />
        ))}
      </div>
    </div>
  );
  /* eslint-enable react-memo/require-usememo */
});
CommandTextField.displayName = 'CommandTextField';
