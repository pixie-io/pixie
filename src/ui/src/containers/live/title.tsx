import clsx from 'clsx';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import ArgsEditor from './args-editor';
import { ScriptContext } from '../../context/script-context';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    color: theme.palette.foreground.one,
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

const LiveViewTitle = (props) => {
  const { id, title } = React.useContext(ScriptContext);
  const classes = useStyles();

  const scriptId = id || 'unknown';
  const desc = title || 'untitled';

  return (
    <div className={clsx(props.className, classes.root)}>
      <div className={classes.title}>{desc}</div>
      <div className={classes.scriptName}>
        script:
        {' '}
        {scriptId}
        <ArgsEditor />
      </div>
    </div>
  );
};

export default LiveViewTitle;
