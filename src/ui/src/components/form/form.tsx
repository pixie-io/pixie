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

// This component renders a form from a JSON object. Useful for Username and Password Authentication.

import * as React from 'react';

import { Button, TextField } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { PixienautBox, PixienautImage } from 'app/components/auth/pixienaut-box';
import { WithChildren } from 'app/utils/react-boilerplate';

const useStyles = makeStyles(({ spacing, typography }: Theme) => createStyles({
  button: {
    padding: spacing(1.5),
    margin: `${spacing(2)} 0 ${spacing(4)}`,
    textTransform: 'uppercase',
  },
  hiddenFormField: {
    display: 'none',
  },
  formField: {
    display: 'block',
    marginBottom: spacing(2),
  },
  inputLabel: {
    textTransform: 'capitalize',
  },
  errorField: {
    color: 'red',
    fontSize: typography.fontSize,
    fontFamily: typography.fontFamily,
    whiteSpace: 'pre',
    marginBottom: spacing(4),
  },
}), { name: 'Form' });

/**
 * Represents a message that should be shown above a field.
 * Modified from ory/kratos-client.
 */
export interface FormFieldMessage {
  // The context object of the message.
  // eslint-disable-next-line
  context?: object;
  // The text of the actual message.
  text?: string;
}

/**
 * Field represents a HTML Form Field.
 * Modified from ory/kratos-client.
 */
export interface FormField {
  // Disabled is the equivalent of `<input {{if .Disabled}}disabled{{end}}\">`
  disabled?: boolean;
  // The messages for this particular input. Might include a description of failures if any occur.
  messages?: Array<FormFieldMessage>;
  //  Name is the equivalent of `<input name=\"{{.Name}}\">`
  name: string;
  // Pattern is the equivalent of `<input pattern=\"{{.Pattern}}\">`
  pattern?: string;
  // Required is the equivalent of `<input required=\"{{.Required}}\">`
  required?: boolean;
  // Type is the equivalent of `<input type=\"{{.Type}}\">`
  type: string;
  // Value is the equivalent of `<input value=\"{{.Value}}\">`
  // eslint-disable-next-line
  value?: object;
}

/**
 * FormStructure represents the full form to be rendered.
 */
export interface FormStructure {
  // The Text to render on the submit button.
  submitBtnText: string;
  // Action should be used as the form action URL `<form action=\"{{ .Action }}\" method=\"post\">`.
  action: string;
  // Form contains multiple fields
  fields: Array<FormField>;
  // Method is the REST method for the form(e.g. POST)
  method: string;
  // Messages that come up when submitting.
  errors?: Array<FormFieldMessage>;
  // Event to happen when the user submits the form. If `defaultSubmit` is false
  // or unspecified, onClick will be run. If 'defaultSubmit` is true, then we'll
  // run onClick and also submit the form.
  onClick?: React.FormEventHandler<HTMLFormElement>;
  // onChange will receive data from a formField whenever that data is updated.
  onChange?: React.ChangeEventHandler<HTMLTextAreaElement | HTMLInputElement>;
  // Submit the form when click submit. If unspecified, the form will not submit.
  defaultSubmit?: boolean;
}

export const composeMessages = (messages?: FormFieldMessage[]): string | null => (
  messages?.map((m) => m.text).join('\n') ?? null
);

interface FormFieldProps extends FormField {
  onChange?: React.ChangeEventHandler<HTMLTextAreaElement | HTMLInputElement>;
}

// eslint-disable-next-line react-memo/require-memo
const FormFieldImpl: React.FC<WithChildren<FormFieldProps>> = ({ onChange, children, ...field }) => {
  const classes = useStyles();
  // TODO(philkuz) figure out how to keep the value set OR wipe the value away beforehand to avoid this.
  // If the value is set beforehand, you can't edit the field.
  const isHidden = field.type === 'hidden';
  const value = field.value && isHidden ? { value: field.value } : {};
  return (
    <TextField
      className={isHidden ? classes.hiddenFormField : classes.formField}
      label={isHidden ? null : field.name}
      // eslint-disable-next-line react-memo/require-usememo
      InputLabelProps={{ className: classes.inputLabel }}
      name={field.name}
      onChange={onChange}
      disabled={field.disabled}
      required={field.required}
      helperText={composeMessages(field.messages)}
      type={field.type}
      {...value}
    />
  );
};
FormFieldImpl.displayName = 'FormFieldImpl';

// eslint-disable-next-line react-memo/require-memo
export const Form: React.FC<FormStructure> = ({
  submitBtnText,
  action,
  fields,
  method,
  onClick,
  onChange,
  defaultSubmit,
  errors,
}) => {
  const classes = useStyles();

  const onSubmit: React.FormEventHandler<HTMLFormElement> = (e) => {
    if (onClick != null) {
      onClick(e);
    }
    if (defaultSubmit == null || !defaultSubmit) {
      e.preventDefault();
    }
  };

  const errorText = composeMessages(errors);

  return (
    <>
      {errorText && (
        <div className={classes.errorField}>
          {' '}
          {errorText}
        </div>
      )}

      <form method={method} action={action} onSubmit={onSubmit}>
        {fields.map((f) => (
          <FormFieldImpl
            key={f.name}
            onChange={onChange}
            {...f}
          />
        ))}

        <Button
          // Note we don't specify onClick, instead call onSubmit.
          className={classes.button}
          variant='contained'
          type='submit'
          color='primary'
          name='method'
          value='password'
        >
          {submitBtnText}
        </Button>
      </form>
    </>
  );
};
Form.displayName = 'Form';

// eslint-disable-next-line react-memo/require-memo
export const PixienautForm: React.FC<{ formProps: FormStructure }> = ({ formProps }) => {
  const hasError = formProps.errors != null;
  const image: PixienautImage = hasError ? 'octopus' : 'balloon';

  return (
    <PixienautBox image={image}>
      <Form {...formProps} />
    </PixienautBox>
  );
};
PixienautForm.displayName = 'PixienautForm';
