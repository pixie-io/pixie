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

import Button from '@material-ui/core/Button';
import { SnackbarProvider, useSnackbar } from './snackbar';

export default {
  title: 'Snackbar',
  component: SnackbarProvider,
  decorators: [
    (Story) => (
      <SnackbarProvider>
        <Story />
      </SnackbarProvider>
    ),
  ],
};

export const Basic = () => {
  const showSnackbar = useSnackbar();

  const click = () => {
    showSnackbar({
      message: 'this is a snackbar',
    });
  };
  return (
    <Button color='primary' onClick={click}>
      Click me
    </Button>
  );
};

export const WithAction = () => {
  const showSnackbar = useSnackbar();

  const click = () => {
    showSnackbar({
      message: 'this is a snackbar',
      action: () => window.setTimeout(click, 1000),
      actionTitle: 'Again',
    });
  };
  return (
    <Button color='primary' onClick={click}>
      Click me
    </Button>
  );
};
