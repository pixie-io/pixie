import {SnackbarProvider, useSnackbar} from 'components/snackbar/snackbar';
import * as React from 'react';

import Button from '@material-ui/core/Button';
import {storiesOf} from '@storybook/react';

storiesOf('Snackbar', module)
  .add('Basic', () => {
    const showSnackbar = useSnackbar();

    const click = () => {
      showSnackbar({
        message: 'this is a snackbar',
      });
    };
    return <Button color='primary' onClick={click}>Click me</Button>;
  }, {
    info: { inline: true },
    note: 'Snackbar component',
    decorators: [(StoryFn) => (
      <SnackbarProvider><StoryFn /></SnackbarProvider>
    )],
  })
  .add('with action', () => {
    const showSnackbar = useSnackbar();

    const click = () => {
      showSnackbar({
        message: 'this is a snackbar',
        action: () => window.setTimeout(click, 1000),
        actionTitle: 'Again',
      });
    };
    return <Button color='primary' onClick={click}>Click me</Button>;
  }, {
    info: { inline: true },
    note: 'Snackbar component',
    decorators: [(StoryFn) => (
      <SnackbarProvider><StoryFn /></SnackbarProvider>
    )],
  });
