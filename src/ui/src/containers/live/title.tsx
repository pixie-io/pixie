import clsx from 'clsx';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import ArgsEditor from './args-editor';
import { TitleContext } from './context';

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
      alignItems: 'center',
    },
  }),
);

const LiveViewTitle = (props) => {
  const title = React.useContext(TitleContext);
  const classes = useStyles();

  // tslint:disable:whitespace remove this once we switch to eslint
  const id = title?.id || 'unknown';
  const desc = title?.title || 'untitled';
  // tslint:enable:whitespace

  return (
    <div className={clsx(props.className, classes.root)}>
      <div className={classes.title}>{desc}</div>
      <div className={classes.scriptName}>
        script: {id}
        <ArgsEditor />
      </div>
    </div>
  );
};

export default LiveViewTitle;
