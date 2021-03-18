import * as React from 'react';
import { Grid } from '@material-ui/core';
import * as QueryString from 'query-string';
import { AuthMessageBox } from '@pixie-labs/components';
import { BasePage } from './base';

export const CLIAuthCompletePage = () => {
  const params = QueryString.parse(window.location.search.substr(1));

  const title = params.err ? 'Authentication Failed' : 'Authentication Complete';
  const message = params.err ? `${params.err}`
    : 'Authentication was successful, please close this page and return to the CLI.';

  return (
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
            <AuthMessageBox
              title={title}
              message={message}
            />
          </Grid>
        </Grid>
      </BasePage>
    </>
  );
};
