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

import { ContentCopy as CopyIcon, Close as CloseIcon } from '@mui/icons-material';
import { IconButton, Tooltip, Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { buildClass, useSnackbar } from 'app/components';
import { JSONData } from 'app/containers/format-data/json-data';

const useDetailPaneClasses = makeStyles((theme: Theme) => createStyles({
  details: {
    position: 'relative',
    flex: 1,
    whiteSpace: 'pre-wrap',
    overflow: 'hidden',
    backgroundColor: theme.palette.background.paper,
    display: 'flex',
    flexFlow: 'column nowrap',
  },
  horizontal: {
    borderTop: `1px solid ${theme.palette.background.six}`,
    minWidth: 0,
    minHeight: theme.spacing(30),
  },
  vertical: {
    borderLeft: `1px solid ${theme.palette.background.six}`,
    minWidth: theme.spacing(30),
    minHeight: 0,
  },
  header: {
    flex: '0 0 auto',
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-between',
    alignItems: 'center',
    position: 'sticky',
    top: 0,
    width: '100%',
    height: theme.spacing(5),
    background: theme.palette.background.paper,
    borderBottom: `1px solid ${theme.palette.background.six}`,
    '& > h4': {
      margin: 0,
      textTransform: 'uppercase',
    },
  },
  content: {
    flex: '1 1 auto',
    padding: theme.spacing(2),
    overflow: 'auto',
  },
}), { name: 'DetailPane' });

interface DetailPaneProps {
  details: Record<string, any> | null;
  closeDetails: () => void;
  splitMode?: 'horizontal' | 'vertical';
}

export const DetailPane = React.memo<DetailPaneProps>(({ details, closeDetails, splitMode = 'vertical' }) => {
  const classes = useDetailPaneClasses();
  const showSnackbar = useSnackbar();

  const copyDetails = React.useCallback(() => {
    if (!details) return;
    const text = JSON.stringify(details, null, 2);
    navigator.clipboard.writeText(text).then(
      () => { showSnackbar({ message: 'Copied!' }); },
      (err) => {
        showSnackbar({ message: 'Error: could not copy details automatically' });
        console.error(err);
      },
    );
  }, [details, showSnackbar]);

  if (!details) return null;
  return (
    <div
      className={buildClass(classes.details, classes[splitMode])}
    >
      <div className={classes.header}>
        <Tooltip title='Copy Details to Clipboard'>
          <IconButton onClick={copyDetails}><CopyIcon /></IconButton>
        </Tooltip>
        <Typography variant='h4'>Details</Typography>
        <IconButton
          aria-label='close details'
          onClick={closeDetails}
        >
          <CloseIcon />
        </IconButton>
      </div>
      <div className={classes.content}>
        <JSONData data={details} multiline />
      </div>
    </div>
  );
});
DetailPane.displayName = 'DetailPane';
