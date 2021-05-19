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

import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

const useStyles = makeStyles((theme: Theme) => createStyles({
  form: {
    ...theme.typography.h6,
    cursor: 'text',
    display: 'flex',
    flexWrap: 'wrap',
    flexDirection: 'row',
  },
  label: {},
  input: {
    background: 'transparent',
    outline: 'none',
    border: 'none',
    color: 'inherit',
  },
}));

interface Form {
  [field: string]: string;
}

type FieldValuePair = [string, string];

function useTabIndexReducer(numElements: number) {
  const reducer = (state, action) => {
    switch (action) {
      case 'next':
        return (state + 1) % numElements;
      case 'prev':
        return (state - 1 + numElements) % numElements;
      default:
        throw new Error('unknown action');
    }
  };
  return React.useReducer(reducer, 0);
}

interface FormFieldProps {
  field: string;
  value: string;
  focus: boolean;
  onValueChange: (val: FieldValuePair) => void;
  dispatch: (action) => void;
}

const FormField: React.FC<FormFieldProps> = ({
  field,
  value,
  focus,
  onValueChange,
  dispatch,
}) => {
  const classes = useStyles();
  const ref = React.useRef(null);
  React.useEffect(() => {
    if (focus) {
      ref.current.focus();
    }
  }, [focus]);

  const onChange = React.useCallback(
    (event) => {
      onValueChange([field, event.target.value]);
    },
    [field, onValueChange],
  );

  const onKeyDown = React.useCallback(
    (event) => {
      if (event.key === 'ArrowLeft' && ref.current.selectionStart === 0) {
        dispatch('prev');
      } else if (
        event.key === 'ArrowRight'
        && ref.current.selectionStart === ref.current.value.length
      ) {
        dispatch('next');
      }
    },
    [dispatch],
  );

  return (
    <>
      <span className={classes.label}>
        {field}
        :&nbsp;
      </span>
      <input
        ref={ref}
        className={classes.input}
        value={value}
        onKeyDown={onKeyDown}
        onChange={onChange}
      />
    </>
  );
};

interface FormFieldInputProps {
  onValueChange: (val: FieldValuePair) => void;
  form: Form;
}

export const FormFieldInput: React.FC<FormFieldInputProps> = ({
  onValueChange,
  form,
}) => {
  const classes = useStyles();
  const formFields = Object.keys(form).map((field) => [field, form[field]]);
  const [focusIndex, dispatch] = useTabIndexReducer(formFields.length);
  const handleKeypress = React.useCallback(
    (event) => {
      if (event.key === 'Tab') {
        event.preventDefault();
        dispatch(event.shiftKey ? 'prev' : 'next');
      }
    },
    [dispatch],
  );

  return (
    <div className={classes.form} onKeyDown={handleKeypress}>
      {formFields.map(([field, value], i) => (
        <FormField
          key={field}
          field={field}
          value={value}
          onValueChange={onValueChange}
          dispatch={dispatch}
          focus={focusIndex === i}
        />
      ))}
    </div>
  );
};
