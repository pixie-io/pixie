import * as React from 'react';
import { Grid } from '@material-ui/core';
import { BasePage } from './base';
import { MessageBox } from '../../components/auth/message';

export const CLIAuthCompletePage = () => (
  <>
    <BasePage>
      <Grid
        container
        direction='row'
        spacing={0}
        justify='space-evenly'
        alignItems='center'
      >
        <Grid item>
          <MessageBox
            title='Authentication Complete'
            message='Authentication was successful, please close this page and return to the CLI.'
          />
        </Grid>
      </Grid>
    </BasePage>
  </>
);
