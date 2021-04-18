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

import { FrameElement } from 'utils/frame-utils';
import { Form, FormStructure, PixienautForm } from './form';

export default {
  title: 'Form',
  component: Form,
  decorators: [
    (Story) => (
      <FrameElement width={500}>
        <Story />
      </FrameElement>

    ),
  ],
};

export const LoginForm = () => {
  const formStruct: FormStructure = {
    submitBtnText: 'Login',
    action: '/path/to/action',
    method: 'POST',
    fields: [{
      name: 'email',
      pattern: 'email',
      required: true,
      type: 'text',
    }, {
      name: 'password',
      required: true,
      type: 'password',
    }, {
      name: 'csrf_token',
      required: true,
      type: 'hidden',
      value: 'abcdef',
    }],
  };
  return (<Form {...formStruct} />);
};

export const LoginFormWithError = () => {
  const formStruct: FormStructure = {
    submitBtnText: 'Login',
    action: '/path/to/action',
    method: 'POST',
    fields: [{
      name: 'email',
      pattern: 'email',
      messages: [{
        text: 'email and password combo not found',
      }],
      required: true,
      type: 'text',
    }, {
      name: 'password',
      required: true,
      type: 'password',
    }],
  };
  return (<Form {...formStruct} />);
};

export const PixienautLoginForm = () => {
  const formStruct: FormStructure = {
    submitBtnText: 'Login',
    action: '/path/to/action',
    method: 'POST',
    fields: [{
      name: 'email',
      pattern: 'email',
      required: true,
      type: 'text',
    }, {
      name: 'password',
      required: true,
      type: 'password',
    }, {
      name: 'csrf_token',
      required: true,
      type: 'hidden',
      value: 'abcdef',
    }],
  };
  return (<PixienautForm hasError={false} formProps={formStruct} />);
};

export const PixienautLoginFormWithError = () => {
  const formStruct: FormStructure = {
    submitBtnText: 'Login',
    action: '/path/to/action',
    method: 'POST',
    errors: [{
      text: 'email and password combo not found',
    }],
    fields: [{
      name: 'email',
      pattern: 'email',
      required: true,
      type: 'text',
    }, {
      name: 'password',
      required: true,
      type: 'password',
    }],
  };
  return (<PixienautForm formProps={formStruct} />);
};
