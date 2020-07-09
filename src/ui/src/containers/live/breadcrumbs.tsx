import clsx from 'clsx';
import * as React from 'react';

import {
  createStyles, Theme, withStyles,
} from '@material-ui/core/styles';

import ArgsEditor from './args-editor';
import { ScriptContext } from '../../context/script-context';

const styles = ((theme: Theme) => createStyles({
  root: {
    color: theme.palette.foreground.one,
    padding: theme.spacing(1),
  },
  title: {
    ...theme.typography.h6,
    fontWeight: theme.typography.fontWeightBold,
  },
  scriptName: {
    display: 'flex',
    flexDirection: 'row',
    ...theme.typography.subtitle2,
    fontWeight: theme.typography.fontWeightLight,
    alignItems: 'center',
  },
}));

const LiveViewBreadcrumbs = ({ classes }) => {
  const { id } = React.useContext(ScriptContext);

  const scriptId = id || 'unknown';

  return (
    <div className={classes.root}>
      <div className={classes.scriptName}>
        script:
        {' '}
        {scriptId}
        <ArgsEditor />
      </div>
    </div>
  );
};

export default withStyles(styles)(LiveViewBreadcrumbs);
