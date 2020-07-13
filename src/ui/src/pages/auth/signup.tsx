import { Grid, Hidden } from '@material-ui/core';
import * as React from 'react';
import { BasePage } from './base';
import { SignupMarcom } from '../../components/auth/signup-marcom';
import { AuthBox } from '../../components/auth/auth-box';
import { auth0SignupRequest } from './utils';

export const SignupPage = () => (
  <>
    <BasePage>
      <Grid
        container
        direction='row'
        spacing={0}
        justify='space-evenly'
        alignItems='center'
      >
        <Hidden smDown>
          <Grid item xs={5}>
            <SignupMarcom />
          </Grid>
        </Hidden>
        <Grid item>
          <AuthBox
            variant='signup'
            toggleURL={`/auth/login${window.location.search}`}
            onPrimaryButtonClick={auth0SignupRequest}
          />
        </Grid>
      </Grid>
    </BasePage>
  </>
);
