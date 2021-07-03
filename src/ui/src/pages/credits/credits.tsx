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

import { buildClass, scrollbarStyles, Footer } from 'app/components';
import { Copyright } from 'configurable/copyright';

import {
  Theme, makeStyles, Button,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';

import * as React from 'react';
import { LiveViewButton } from 'app/containers/admin/utils';
import NavBars from 'app/containers/App/nav-bars';

import licenseJson from 'configurable/licenses.json';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  title: {
    flexGrow: 1,
    marginLeft: theme.spacing(2),
  },
  row: {
    width: '100%',
    float: 'left',
  },
  main: {
    overflow: 'auto',
    marginLeft: theme.spacing(6),
    flex: 1,
    minHeight: 0,
    borderTopStyle: 'solid',
    borderTopColor: theme.palette.background.three,
    borderTopWidth: theme.spacing(0.25),
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
  topbarTitle: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    fontWeight: theme.typography.fontWeightBold,
    display: 'flex',
    alignItems: 'center',
    height: '100%',
  },
  floatLeft: {
    float: 'left',
  },
  header: {
    textAlign: 'center',
  },
  container: {
    maxWidth: '1290px',
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
}));

interface LicenseEntry {
  name: string;
  spdxID?: string;
  url?: string;
  licenseText?: string;
}

const CreditsPage: React.FC = ({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <NavBars>
        <div className={classes.title}>
          <div className={classes.topbarTitle}>Credits</div>
        </div>
        <LiveViewButton />
      </NavBars>
      <div className={classes.main}>
        {children}
      </div>
    </div>
  );
};

const LicenseEntryRow: React.FC<LicenseEntry> = ({ name, url, licenseText }) => {
  const classes = useStyles();
  const [showLicense, setShowLicense] = React.useState(false);
  return (
    <div className={classes.row}>
      <div className={classes.floatLeft}>
        {name}
      </div>
      <div className={classes.creditsShow}>
        <Button
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
          <div className={classes.licenseBody}>
            <br />
            <pre>
              {licenseText}
            </pre>
          </div>
        ) : null}
    </div>
  );
};

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
      <div className={classes.container}>
        {licenses.map((license: LicenseEntry) => <LicenseEntryRow {...license} key={license.name} />)}
      </div>
    </>
  );
};

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

export default CreditsOverviewPage;
