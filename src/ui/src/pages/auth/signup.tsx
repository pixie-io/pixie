import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core';
import * as React from 'react';
import { AuthBox, SignupMarcom } from '@pixie/components';
import { BasePage } from './base';
import { OAuthSignupRequest } from './utils';

const styles = ({ breakpoints }: Theme) => createStyles({
  root: {
    width: '100%',
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-evenly',
    alignItems: 'center',
  },
  marketingBlurb: {
    [breakpoints.down('sm')]: {
      display: 'none',
    },
  },
});

export const SignupPage = withStyles(styles)(({ classes }: WithStyles<typeof styles>) => (
  <BasePage>
    <div className={classes.root}>
      <div className={classes.marketingBlurb}>
        <SignupMarcom />
      </div>
      <div>
        <AuthBox
          variant='signup'
          toggleURL={`/auth/login${window.location.search}`}
          onPrimaryButtonClick={OAuthSignupRequest}
          showTOSDisclaimer
        />
      </div>
    </div>
  </BasePage>
));
