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
  DeleteForeverOutlined as DeleteIcon,
  CopyAllOutlined as CopyIcon,
} from '@mui/icons-material';
import { IconButton } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

export const useKeyListStyles = makeStyles((theme: Theme) => createStyles({
  table: {
    width: '100%',
    maxWidth: theme.breakpoints.values.lg,
    margin: '0 auto',
  },
  tableHeadRow: {
    '& > th': {
      fontWeight: 'normal',
      textTransform: 'uppercase',
      color: theme.palette.foreground.grey4,
    },
  },
  keyValue: {
    padding: 0,
    fontWeight: theme.typography.fontWeightLight,
    fontSize: theme.typography.body2.fontSize,
    color: theme.palette.foreground.one,
    borderWidth: theme.spacing(1),
    borderColor: theme.palette.background.default,
  },
  actionsButton: {
    padding: theme.spacing(0.75),
  },
  copyBtn: {
    minWidth: theme.spacing(3.75),
  },
  error: {
    padding: theme.spacing(1),
  },
}), { name: 'KeyList' });

export const KeyActionButtons: React.FC<{
  deleteOnClick: () => void;
  copyOnClick: () => void;
  // eslint-disable-next-line react-memo/require-memo
}> = ({
  deleteOnClick, copyOnClick,
}) => {
  const classes = useKeyListStyles();
  return (
    <>
      <IconButton onClick={copyOnClick} className={classes.actionsButton}>
        <CopyIcon />
      </IconButton>
      <IconButton onClick={deleteOnClick} className={classes.actionsButton}>
        <DeleteIcon />
      </IconButton>
    </>
  );
};
KeyActionButtons.displayName = 'KeyActionButtons';
