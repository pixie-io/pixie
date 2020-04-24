import clsx from 'clsx';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import {ArgsContext, TitleContext} from './context';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
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
    },
  }),
);

const LiveViewTitle = (props) => {
  const title = React.useContext(TitleContext);
  const { args } = React.useContext(ArgsContext);
  const classes = useStyles();
  const argsList = [];
  if (args) {
    for (const argName of Object.keys(args)) {
      if (argName === 'script') {
        continue;
      }
      const argVal = args[argName];
      argsList.push(<div key={argName}>&nbsp;&nbsp;{argName}: {argVal}</div>);
    }
  }
  return (
    <div className={clsx(props.className, classes.root)}>
      <div className={classes.title}>{title.title}</div>
      <div className={classes.scriptName}>
        script: {title.id}
        {argsList}
      </div>
    </div>
  );
};

export default LiveViewTitle;
