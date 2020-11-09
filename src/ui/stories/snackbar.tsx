import * as React from 'react';

import Button from '@material-ui/core/Button';
import { SnackbarProvider, useSnackbar } from 'components/snackbar/snackbar';

export default {
  title: 'Snackbar',
  component: SnackbarProvider,
  decorators: [(Story) => <SnackbarProvider><Story /></SnackbarProvider>],
};

export const Basic = () => {
  const showSnackbar = useSnackbar();

  const click = () => {
    showSnackbar({
      message: 'this is a snackbar',
    });
  };
  return <Button color='primary' onClick={click}>Click me</Button>;
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
  return <Button color='primary' onClick={click}>Click me</Button>;
};
