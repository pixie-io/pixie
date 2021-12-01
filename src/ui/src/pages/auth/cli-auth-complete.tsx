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

import { Grid } from '@mui/material';
import * as QueryString from 'query-string';

import { AuthMessageBox } from 'app/components';

import { BasePage } from './base';

// eslint-disable-next-line react-memo/require-memo
export const CLIAuthCompletePage: React.FC = () => {
  const params = QueryString.parse(window.location.search.substr(1));

  const title = params.err ? 'Authentication Failed' : 'Authentication Complete';
  const message = params.err ? `${params.err}`
    : 'Authentication was successful, please close this page and return to the CLI.';

  return (
    <>
      <BasePage>
        <Grid
          container
          direction='row'
          spacing={0}
          justifyContent='space-evenly'
          alignItems='center'
        >
          <Grid item>
            <AuthMessageBox
              title={title}
              message={message}
            />
          </Grid>
        </Grid>
      </BasePage>
    </>
  );
};
CLIAuthCompletePage.displayName = 'CLIAuthCompletePage';
