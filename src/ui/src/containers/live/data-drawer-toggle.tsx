import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import ExpandLessIcon from '@material-ui/icons/ExpandLess';
import ExpandMoreIcon from '@material-ui/icons/ExpandMore';

import {ResultsContext} from './context';

interface DataDrawerToggleProps {
  opened: boolean;
  toggle: () => void;
}

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    root: {
      display: 'flex',
      flexDirection: 'row',
      height: theme.spacing(5),
      paddingLeft: theme.spacing(2),
      alignItems: 'center',
      backgroundColor: theme.palette.background.three,
      cursor: 'pointer',
    },
    title: {
      ...theme.typography.subtitle2,
      color: theme.palette.foreground.one,
      marginLeft: theme.spacing(2),
    },
  });
});

const DataDrawerToggle = (props: DataDrawerToggleProps) => {
  const { opened, toggle } = props;
  const classes = useStyles();
  const { error } = React.useContext(ResultsContext);

  return (
    <div className={classes.root} onClick={toggle}>
      {opened ? <ExpandMoreIcon /> : <ExpandLessIcon />}
      <div className={classes.title}>{error ? 'Error Details' : 'Underlying Data'}</div>
    </div>
  );
};

export default DataDrawerToggle;
