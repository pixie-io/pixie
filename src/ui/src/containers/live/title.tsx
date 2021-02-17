import { buildClass } from '@pixie/components';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

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
  const { title } = React.useContext(ScriptContext);
  const classes = useStyles();

  const desc = title || 'untitled';

  return (
    <div className={buildClass(props.className, classes.root)}>
      <div className={classes.title}>{desc}</div>
    </div>
  );
};

export default LiveViewTitle;
