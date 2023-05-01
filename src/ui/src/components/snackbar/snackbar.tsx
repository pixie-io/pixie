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

import { Close as CloseIcon } from '@mui/icons-material';
import { Button, IconButton, Snackbar } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { WithChildren } from 'app/utils/react-boilerplate';

const noop = () => {};

interface ShowArgs {
  message: string;
  action?: () => void;
  actionTitle?: string;
  autoHideDuration?: number;
  dismissible?: boolean;
}

type ShowSnackbarFunc = (args: ShowArgs) => void;

export const SnackbarContext = React.createContext<ShowSnackbarFunc>(null);
SnackbarContext.displayName = 'SnackbarContext';

type SnackbarState = {
  opened: boolean;
} & Required<ShowArgs>;

const useStyles = makeStyles((theme: Theme) => createStyles({
  snackbar: {
    backgroundColor: theme.palette.background.five,
    color: theme.palette.text.secondary,
  },
}), { name: 'Snackbar' });

const useSnackbarStyles = makeStyles(createStyles({
  message: {
    whiteSpace: 'pre-wrap',
  },
}), { name: 'SnackbarInner' });

// eslint-disable-next-line react-memo/require-memo
export const SnackbarProvider: React.FC<WithChildren> = ({ children }) => {
  const classes = useStyles();
  const snackbarClasses = useSnackbarStyles();
  const [state, setState] = React.useState<SnackbarState>({
    opened: false,
    message: '',
    action: noop,
    actionTitle: '',
    autoHideDuration: 2000,
    dismissible: true,
  });

  const showSnackbar = React.useCallback((args: ShowArgs) => {
    const {
      message,
      action = noop,
      actionTitle = '',
      autoHideDuration = 2000,
      dismissible = true,
    } = args;
    setState({
      message,
      action,
      actionTitle,
      autoHideDuration,
      dismissible,
      opened: true,
    });
  }, []);

  const hideSnackbar = React.useCallback(() => {
    setState((prevState) => ({
      ...prevState,
      opened: false,
    }));
  }, []);

  const snackbarAction = React.useMemo(
    () => (
      <>
        {state.action !== noop && state.actionTitle && (
          <Button
            // eslint-disable-next-line react-memo/require-usememo
            onClick={() => {
              state.action();
              hideSnackbar();
            }}
          >
            {state.actionTitle}
          </Button>
        )}
        {state.dismissible && (
          <IconButton onClick={hideSnackbar} color='inherit'>
            <CloseIcon />
          </IconButton>
        )}
      </>
    ),
    [state, hideSnackbar],
  );
  return (
    <>
      <SnackbarContext.Provider value={showSnackbar}>
        {children}
      </SnackbarContext.Provider>
      <Snackbar
        // eslint-disable-next-line react-memo/require-usememo
        anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
        // eslint-disable-next-line react-memo/require-usememo
        ContentProps={{ className: classes.snackbar, classes: snackbarClasses }}
        open={state.opened}
        onClose={hideSnackbar}
        message={state.message}
        action={snackbarAction}
        autoHideDuration={state.autoHideDuration}
      />
    </>
  );
};
SnackbarProvider.displayName = 'SnackbarProvider';

export function useSnackbar(): ShowSnackbarFunc {
  const show = React.useContext(SnackbarContext);
  if (!show) {
    throw new Error('useSnackbar must be call from within SnackbarProvider');
  }
  return show;
}
