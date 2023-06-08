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

import { Button } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { buildClass, scrollbarStyles, Footer } from 'app/components';
import { WithChildren } from 'app/utils/react-boilerplate';
import { Copyright } from 'configurable/copyright';

import licenseJson from './licenses.json';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  row: {
    width: '100%',
    float: 'left',
  },
  main: {
    overflow: 'auto',
    marginLeft: theme.spacing(8),
    flex: 1,
    minHeight: 0,
    padding: theme.spacing(1),
    display: 'flex',
    flexFlow: 'column nowrap',
  },
  mainBlock: {
    flex: '1 0 auto',
  },
  mainFooter: {
    flex: '0 0 auto',
  },
  titleText: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    fontWeight: theme.typography.fontWeightBold,
  },
  floatLeft: {
    float: 'left',
  },
  header: {
    textAlign: 'center',
  },
  container: {
    maxWidth: theme.breakpoints.values.lg,
    marginLeft: 'auto',
    marginRight: 'auto',
    width: '80%',
  },
  creditsShow: {
    float: 'right',
    marginLeft: theme.spacing(3),
    marginRight: theme.spacing(3),
  },
  licenseBody: {
    paddingLeft: theme.spacing(20),
  },
  button: {
    padding: 0,
    '&.hidden': {
      visibility: 'hidden',
    },
  },
}), { name: 'Credits' });

interface LicenseEntry {
  name: string;
  spdxID?: string;
  url?: string;
  licenseText?: string;
}

// eslint-disable-next-line react-memo/require-memo
const CreditsPage: React.FC<WithChildren> = ({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <div className={classes.main}>
        {children}
      </div>
    </div>
  );
};
CreditsPage.displayName = 'CreditsPage';

const LicenseEntryRow = React.memo<LicenseEntry>(({ name, url, licenseText }) => {
  const classes = useStyles();
  const [showLicense, setShowLicense] = React.useState(false);
  return (
    <div className={classes.row} role='listitem'>
      <div className={classes.floatLeft}>
        {name}
      </div>
      <div className={classes.creditsShow}>
        <Button
          // eslint-disable-next-line react-memo/require-usememo
          onClick={() => setShowLicense((show) => !show)}
          className={buildClass(classes.button, !licenseText && 'hidden')}
        >
          <div>
            {showLicense ? 'hide' : 'show'}
            {' '}
          </div>
        </Button>
        <Button href={url} className={buildClass(classes.button, !url && 'hidden')}>homepage</Button>
      </div>
      {showLicense
        ? (
          <div className={classes.licenseBody} role='document'>
            <br />
            <pre>
              {licenseText}
            </pre>
          </div>
        ) : null}
    </div>
  );
});
LicenseEntryRow.displayName = 'LicenseEntryRow';

// eslint-disable-next-line react-memo/require-memo
const Credits: React.FC<{ licenses: LicenseEntry[] }> = ({ licenses }) => {
  const classes = useStyles();

  if (!licenses) {
    return (<div> not found </div>);
  }

  return (
    <>
      <div className={`${classes.titleText}  ${classes.header}`}>
        <h1>Credits</h1>
        <h4>Third party packages we use and love.</h4>
      </div>
      <div className={classes.container} role='list'>
        {licenses.map((license: LicenseEntry) => <LicenseEntryRow {...license} key={license.name} />)}
      </div>
    </>
  );
};
Credits.displayName = 'Credits';

// eslint-disable-next-line react-memo/require-memo
const CreditsOverviewPage: React.FC = () => {
  const classes = useStyles();
  return (
    <CreditsPage>
      <div className={classes.mainBlock}>
        <Credits licenses={licenseJson} />
      </div>
      <div className={classes.mainFooter}>
        <Footer copyright={Copyright} />
      </div>
    </CreditsPage>
  );
};
CreditsOverviewPage.displayName = 'CreditsOverviewPage';

export default CreditsOverviewPage;
