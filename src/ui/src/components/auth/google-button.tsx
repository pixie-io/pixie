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
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { GoogleIcon } from 'app/components/icons/google';

const useStyles = makeStyles(({ spacing }: Theme) => createStyles({
  button: {
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    textTransform: 'capitalize',
  },
}), { name: 'GoogleButton' });

export interface GoogleButtonProps {
  text: string;
  onClick: () => void;
}

export const GoogleButton = React.memo<GoogleButtonProps>(({ text, onClick }) => {
  const classes = useStyles();
  return (
    <Button
      variant='contained'
      color='primary'
      className={classes.button}
      // eslint-disable-next-line react-memo/require-usememo
      startIcon={<GoogleIcon />}
      onClick={onClick}
    >
      {text}
    </Button>
  );
});
GoogleButton.displayName = 'GoogleButton';
