import { scrollbarStyles } from 'common/mui-theme';
import { StyleRulesCallback, Theme, withStyles } from '@material-ui/core/styles';
import Button from '@material-ui/core/Button';

import * as React from 'react';
import { Route, Router, Switch } from 'react-router-dom';
import { LiveViewButton } from 'containers/admin/utils';
import NavBars from 'containers/App/nav-bars';
import history from 'utils/pl-history';

import licenseJson from './licenses.json';

const styles: StyleRulesCallback<Theme, {}> = (theme: Theme) => ({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    backgroundColor: theme.palette.background.default,
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
  },
});

const CreditsPage = withStyles(styles)(({ children, classes }: any) => (
  <div className={classes.root}>
    <NavBars>
      <div className={classes.title}>
        <div className={classes.titleText}>Credits</div>
      </div>
      <LiveViewButton />
    </NavBars>
    <div className={classes.main}>
      {children}
    </div>
  </div>
));

const LicenseEntryRow = withStyles(styles)(({
  name, content, url, classes,
}: LicenseEntry & { classes }) => {
  const [showLicense, setShowLicense] = React.useState(false);
  return (
    <div className={classes.row}>
      <div className={classes.floatLeft}>
        {name}
      </div>
      <div className={classes.creditsShow}>
        <Button color='primary' onClick={() => setShowLicense((show) => !show)} className={classes.button}>
          <div>
            {showLicense ? 'hide' : 'show'}
            {' '}
          </div>
        </Button>
        <Button href={url} color='primary' className={classes.button}>homepage</Button>
      </div>
      {showLicense
        ? (
          <div className={classes.licenseBody}>
            <br />
            <pre>
              {content}
            </pre>
          </div>
        ) : null}
    </div>
  );
});

interface LicenseEntry {
  name: string;
  url: string;
  content: string;
}

const Credits = withStyles(styles)(({ licenses, classes }: any) => {
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
});

const CreditsOverviewPage = () => (
  <CreditsPage>
    <Credits licenses={licenseJson} />
  </CreditsPage>
);

export default function CreditsView() {
  return (
    <Router history={history}>
      <Switch>
        <Route exact path='/credits' component={CreditsOverviewPage} />
      </Switch>
    </Router>
  );
}
