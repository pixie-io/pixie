import clsx from 'clsx';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import {TitleContext} from './context';

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
      ...theme.typography.subtitle2,
      fontWeight: theme.typography.fontWeightLight,
    },
  }),
);

const LiveViewTitle = (props) => {
  const title = React.useContext(TitleContext);
  const classes = useStyles();
  return (
    <div className={clsx(props.className, classes.root)}>
      <div className={classes.title}>{title.title}</div>
      <div className={classes.scriptName}>script: {title.id}</div>
    </div>
  );
};

export default LiveViewTitle;
