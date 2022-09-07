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

import { Button, Paper } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { CodeRenderer, Spinner } from 'app/components';
import NavBars from 'app/containers/App/nav-bars';
import { Logo } from 'configurable/logo';

const useStyles = makeStyles((theme: Theme) => createStyles({
  dialog: {
    width: theme.spacing(87.5), // '700px'
  },
  content: {
    padding: theme.spacing(6),
    color: theme.palette.foreground.one,
  },
  container: {
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    width: '100%',
  },
  root: {
    display: 'flex',
    alignItems: 'center',
    width: '100%',
    flexDirection: 'column',
    height: '100%',
  },
  header: {
    ...theme.typography.h5,
    color: theme.palette.foreground.two,
  },
  instructions: {
    marginTop: theme.spacing(3),
    ...theme.typography.body1,
    fontFamily: theme.typography.monospace.fontFamily,
  },
  linksHeader: {
    ...theme.typography.body1,
    color: theme.palette.foreground.two,
    marginTop: theme.spacing(5),
    marginBottom: theme.spacing(2),
  },
  listItem: {
    marginBottom: theme.spacing(1),
    '&::before': {
      content: '"-"',
      marginRight: theme.spacing(0.5),
    },
  },
  linkItem: {
    ...theme.typography.subtitle1,
    fontFamily: theme.typography.monospace.fontFamily,
    color: theme.palette.foreground.one,
    textDecoration: 'underline',
  },
  instructionLink: {
    fontFamily: theme.typography.monospace.fontFamily,
    color: theme.palette.foreground.one,
    textDecoration: 'underline',
  },
  list: {
    listStyle: 'none',
    paddingLeft: 0,
  },
  buttons: {
    display: 'flex',
    flex: 1,
    justifyContent: 'center',
    marginTop: theme.spacing(6),
    marginBottom: theme.spacing(1),
  },
  button: {
    margin: theme.spacing(3),
  },
  logo: {
    float: 'right',
    marginRight: theme.spacing(2),
    marginBottom: theme.spacing(2),
    height: theme.spacing(2),
  },
  centered: {
    display: 'flex',
    alignItems: 'center',
    flexDirection: 'column',
  },
  centeredDialog: {
    display: 'flex',
    alignItems: 'center',
    flexDirection: 'column',
    height: '100%',
    width: '100%',
    justifyContent: 'center',
  },
}), { name: 'DeployInstructions' });

export const DeployInstructions = React.memo(() => {
  const classes = useStyles();

  return (
    <div className={classes.root}>
      <NavBars />
      <div className={classes.centeredDialog}>
        <Paper className={classes.dialog} elevation={1}>
          <div className={classes.content}>
            <span className={classes.header}>Install Pixie</span>
            <CodeRenderer
              code={`bash -c "$(curl -fsSL ${window.location.origin}/install.sh)"`}
              language='bash'
            />
            <div className={classes.instructions}>
              Run this in a macOS Terminal or Linux shell to install Pixie in your K8s cluster.
              Share with your admin if you don&apos;t have access.
              <br />
              <br />
              <span>
                Or, click&nbsp;
                <a className={classes.instructionLink} href='/docs/installing-pixie/quick-start/'>here</a>
                &nbsp;for more options for installing the CLI.
              </span>
            </div>
            <div className={classes.linksHeader}>Don&apos;t have K8s?</div>
            <ul className={classes.list}>
              <li className={classes.listItem}>
                <a className={classes.linkItem} href='/docs/installing-pixie/install-guides'>
                  Set up a quick local K8s sandbox
                </a>
              </li>
              <li className={classes.listItem}>
                <a className={classes.linkItem} href='/docs/installing-pixie/quick-start'>Set up a demo app</a>
              </li>
            </ul>
            <div className={classes.buttons}>
              <Button
                className={classes.button}
                href='https://slackin.px.dev/'
                variant='outlined'
                size='large'
              >
                Slack
              </Button>
              <Button className={classes.button} href='/docs' variant='outlined' size='large'>
                Docs
              </Button>
              <Button
                className={classes.button}
                href='https://github.com/pixie-io/pixie'
                variant='outlined'
                size='large'
              >
                Github
              </Button>
            </div>
          </div>
          <div className={classes.logo}><Logo color='white' /></div>
        </Paper>
      </div>
    </div>
  );
});
DeployInstructions.displayName = 'DeployInstructions';

interface ClusterInstructionsProps {
  message: string;
}

export const ClusterInstructions = React.memo<ClusterInstructionsProps>(({ message }) => {
  const classes = useStyles();

  return (
    <div className={classes.container}>
      <Paper className={classes.dialog} elevation={1}>
        <div className={classes.content}>
          <div className={classes.centered}>
            <p>{message}</p>
            <Spinner />
          </div>
        </div>
        <div className={classes.logo}><Logo color='white' /></div>
      </Paper>
    </div>
  );
});
ClusterInstructions.displayName = 'ClusterInstructions';
