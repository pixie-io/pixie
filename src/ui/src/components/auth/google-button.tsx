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

import { Button } from '@mui/material';

import { GoogleIcon } from 'app/components/icons/google';

export const GoogleButton = React.memo<{ text: string; onClick: () => void }>(({ text, onClick }) => (
  <Button
    variant='contained'
    color='primary'
    // eslint-disable-next-line react-memo/require-usememo
    startIcon={<GoogleIcon />}
    onClick={onClick}
    // eslint-disable-next-line react-memo/require-usememo
    sx={{ pt: 1, pb: 1, textTransform: 'capitalize' }}
  >
    {text}
  </Button>
));
GoogleButton.displayName = 'GoogleButton';
