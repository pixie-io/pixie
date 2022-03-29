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

import { Box, Button, FormControlLabel, Link, Switch, Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { Footer, scrollbarStyles } from 'app/components';
import NavBars from 'app/containers/App/nav-bars';
import { SidebarContext } from 'app/context/sidebar-context';
import * as pixienautCarryingBoxes from 'assets/images/pixienaut-carrying-boxes.svg';
import { Copyright } from 'configurable/copyright';

import { ConfigureDataExportBody } from './data-export-tables';

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
    position: 'relative',
  },
  mainFooter: {
    flex: '0 0 auto',
  },
  splashBlock: {
    width: '100%',
    height: '100%',
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'center',
    alignItems: 'center',
    textAlign: 'center',

    '& > *': {
      maxWidth: theme.breakpoints.values.sm,
      margin: `${theme.spacing(4)} 0`,
    },
  },
}), { name: 'ConfigureDataExportView' });

const ConfigureDataExportPage = React.memo(({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <SidebarContext.Provider value={{ showLiveOptions: false, showAdmin: true }}>
        <NavBars>
          <div className={classes.title}>
            <div className={classes.titleText}>Long-term Data Export</div>
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
ConfigureDataExportPage.displayName = 'ConfigureDataExportPage';

const NoPluginsEnabledSplash = React.memo(() => {
  const classes = useStyles();
  return (
    <div className={classes.splashBlock}>
      <img src={pixienautCarryingBoxes} alt='Long-term Data Export Setup' />
      <Typography variant='body2'>
        Pixie only guarantees data retention for 24 hours.
        <br />
        Configure a <Link href='/admin/plugins'>plugin</Link> to export and store Pixie data for longer term retention.
        <br />
        This data will be accessible and queryable through the plugin provider.
      </Typography>
      <Button variant='contained'>Configure Plugins</Button>
    </div>
  );
});
NoPluginsEnabledSplash.displayName = 'NoPluginsEnabledSplash';

// TODO(nick,PC-1440): Instead of this, check the API (once implemented).
const SplashToggle = React.memo<{
  isSplash: boolean,
  setIsSplash: React.Dispatch<React.SetStateAction<boolean>>,
}>(({
  isSplash, setIsSplash,
}) => {
  return (
    /* eslint-disable react-memo/require-usememo */
    <Box sx={{ position: 'absolute', top: 0, right: 0 }}>
      <FormControlLabel label='DEBUG: Have Plugins' control={
        <Switch checked={!isSplash} onChange={(_, checked) => setIsSplash(!checked)} />
      } />
    </Box>
    /* eslint-enable react-memo/require-usememo */
  );
});
SplashToggle.displayName = 'SplashToggle';

export const ConfigureDataExportView = React.memo(() => {
  const [isSplash, setIsSplash] = React.useState(false);
  return (
    <ConfigureDataExportPage>
      <SplashToggle isSplash={isSplash} setIsSplash={setIsSplash} />
      {isSplash && <NoPluginsEnabledSplash /> }
      {!isSplash && <ConfigureDataExportBody /> }
    </ConfigureDataExportPage>
  );
});
ConfigureDataExportView.displayName = 'ConfigureDataExportView';
