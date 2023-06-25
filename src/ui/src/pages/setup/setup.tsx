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

import { gql, useMutation } from '@apollo/client';
import { Button, TextField, Paper } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import Axios from 'axios';
import * as QueryString from 'query-string';
import { Redirect, useLocation } from 'react-router';

import { Footer, scrollbarStyles } from 'app/components';
import { Spinner } from 'app/components/spinner/spinner';
import NavBars from 'app/containers/App/nav-bars';
import { SidebarContext } from 'app/context/sidebar-context';
import { WithChildren } from 'app/utils/react-boilerplate';
import * as pixienautSetup from 'assets/images/pixienaut-setup.svg';
import { Copyright } from 'configurable/copyright';
import { useCreateOrgExtras } from 'configurable/create-org-extras';

function useRedirectUri(): string {
  const { search } = useLocation();
  const { redirect_uri: redirectParam } = QueryString.parse(search);
  const uri = Array.isArray(redirectParam) ? redirectParam[0] : redirectParam;
  return uri || '/';
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    width: '100%',
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    ...scrollbarStyles(theme),
  },
  title: {
    flexGrow: 1,
    marginLeft: theme.spacing(2),
    height: '100%',
  },
  titleText: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    fontWeight: theme.typography.fontWeightBold,
    display: 'flex',
    alignItems: 'center',
    height: '100%',
  },
  main: {
    marginLeft: theme.spacing(8),
    flex: 1,
    minHeight: 0,
    padding: theme.spacing(1),
    display: 'flex',
    flexFlow: 'column nowrap',
    overflow: 'auto',
  },
  mainBlock: {
    flex: '1 0 auto',
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
  },
  mainFooter: {
    flex: '0 0 auto',
  },
  paper: {
    padding: theme.spacing(6),
    paddingTop: theme.spacing(4),
    borderRadius: theme.shape.borderRadius,
    maxWidth: theme.breakpoints.values.sm,

    '& figure': {
      display: 'flex',
      justifyContent: 'center',
      alignItems: 'center',
      padding: theme.spacing(3),
    },

    '& h1': {
      ...theme.typography.h1,
      fontSize: theme.typography.h2.fontSize,
    },
    '& p:not(.MuiFormHelperText-root)': {
      ...theme.typography.body1,
      color: theme.palette.foreground.one,
      fontSize: theme.typography.h3.fontSize,
      marginTop: theme.spacing(3),
      marginBottom: theme.spacing(3),
      lineHeight: theme.spacing(4),
    },
  },
  inputContainer: {
    paddingTop: theme.spacing(3),
    display: 'flex',
    justifyContent: 'center',
  },
  extrasWrapper: {
    display: 'flex',
    flexFlow: 'column nowrap',
    alignItems: 'center',
    paddingTop: theme.spacing(2),
  },
  buttons: {
    display: 'flex',
    justifyContent: 'center',
    paddingTop: theme.spacing(2),
  },
  muted: {
    opacity: 0.5,
    fontStyle: 'italic',
  },
}), { name: 'SetupView' });

const SetupPage = React.memo<WithChildren>(({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <SidebarContext.Provider value={{ showLiveOptions: false, showAdmin: false }}>
        <NavBars>
          <div className={classes.title}>
            <div className={classes.titleText}>Setup</div>
          </div>
        </NavBars>
      </SidebarContext.Provider>
      <div className={classes.main}>
        <div className={classes.mainBlock}>
          {children}
        </div>
        <div className={classes.mainFooter}>
          <Footer copyright={Copyright} />
        </div>
      </div>
    </div>
  );
});
SetupPage.displayName = 'SetupPage';

export const CREATE_ORG_GQL = gql`
  mutation CreateOrgFromSetupOrgPage($orgName: String!) {
    CreateOrg(orgName: $orgName)
  }
`;

const SetupOrganization = React.memo<{ redirectUri: string }>(({ redirectUri }) => {
  const classes = useStyles();

  const [createOrgError, setCreateOrgError] = React.useState('');
  const [inputValue, setInputValue] = React.useState('');
  const onInputChange = React.useCallback((event) => {
    setCreateOrgError('');
    setInputValue(event.target.value);
  }, [setInputValue, setCreateOrgError]);

  const [valid, validationMessage] = React.useMemo(() => {
    if (!inputValue.trim().length) {
      return [false, ''];
    }
    if (inputValue.trim().length <= 5) {
      return [false, 'Name is too short'];
    }
    if (inputValue.trim().length > 50) {
      return [false, 'Name is too long'];
    }
    if (inputValue.match(/[.$@/\\]/g)) {
      return [false, 'Name must not contain special characters (ex. ./\\$@)'];
    }
    return [true, ''];
  }, [inputValue]);

  const [createOrgMutation] = useMutation<{ CreateOrg: string }, { orgName: string }>(
    CREATE_ORG_GQL,
  );

  const [creating, setCreating] = React.useState(false);
  const extras = useCreateOrgExtras(creating);

  const createOrg = React.useCallback(async () => {
    if (!valid) return;

    setCreating(true);
    try {
      await extras.beforeCreate();
      await createOrgMutation({
        variables: { orgName: inputValue.trim() },
      });
      await Axios.post('/api/auth/refetch');
      await extras.afterCreate();
      setCreating(false);
      window.location.href = redirectUri;
    } catch (error) {
      setCreateOrgError(error.message);
    }
    setCreating(false);
  }, [extras, createOrgMutation, inputValue, redirectUri, setCreateOrgError, valid]);

  const onSubmit = React.useCallback((event: React.FormEvent) => {
    createOrg();
    // So that using Enter to submit the form doesn't force reload the page.
    event.preventDefault();
    return false;
  }, [createOrg]);

  return (
    <Paper elevation={1} className={classes.paper}>
      <form onSubmit={onSubmit} aria-label='Create Your Organization'>
        <h1>Create Your Organization</h1>
        <figure>
          <img src={pixienautSetup} alt='Setup' />
        </figure>
        <p>
          Organizations allow you to collaborate with others by sharing clusters, PxL scripts, and more.
        </p>
        <p>
          <strong>Give your organization a name to get started.</strong>
        </p>
        <div className={classes.inputContainer}>
          <TextField
            variant='outlined'
            error={((!valid && inputValue.length > 0) || !!createOrgError)}
            label='Organization Name'
            helperText={createOrgError || validationMessage}
            value={inputValue}
            onChange={onInputChange}
          />
        </div>
        <p className={classes.muted}>
          Trying to join an organization? Please ask the organization admin for an invite, and check your email.
        </p>
        {extras.infixComponent && <div className={classes.extrasWrapper}>{extras.infixComponent}</div>}
        <div className={classes.buttons}>
          <Button
            variant='contained'
            color='primary'
            onClick={createOrg}
            disabled={creating || !valid || !extras.valid || !inputValue.length}
            endIcon={creating ? <Spinner /> : null}
          >
            { (creating && (extras.loadingText || 'Creating')) || 'Create' }
          </Button>
        </div>
      </form>
    </Paper>
  );
});
SetupOrganization.displayName = 'SetupOrganization';

/** Route to this page when the user needs to perform some first-time setup before they can use Pixie. */
export const SetupView = React.memo(() => {
  const redirectUri = useRedirectUri();

  return (
    <SetupPage>
      <SetupOrganization redirectUri={redirectUri} />
    </SetupPage>
  );
});
SetupView.displayName = 'SetupView';

/** Use as the target of a route when setup should be skipped, and immediately follow its redirect_uri query param. */
export const SetupRedirect = React.memo(() => {
  const redirectUri = useRedirectUri();

  return <Redirect to={redirectUri} />;
});
SetupRedirect.displayName = 'SetupRedirect';
