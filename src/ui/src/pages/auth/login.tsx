import * as React from 'react';
import { Grid } from '@material-ui/core';
import { BasePage } from './base';
import { AuthBox } from '../../components/auth/auth-box';
import { auth0LoginRequest } from './utils';

export const LoginPage = () => (
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
          <AuthBox
            variant='login'
            toggleURL={`/auth/signup${window.location.search}`}
            onPrimaryButtonClick={auth0LoginRequest}
          />
        </Grid>
      </Grid>
    </BasePage>
  </>
);
