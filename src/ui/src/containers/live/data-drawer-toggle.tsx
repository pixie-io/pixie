import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import ExpandLessIcon from '@material-ui/icons/ExpandLess';
import ExpandMoreIcon from '@material-ui/icons/ExpandMore';

interface DataDrawerToggleProps {
  opened: boolean;
  toggle: () => void;
}

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    root: {
      display: 'flex',
      flexDirection: 'row',
      paddingLeft: theme.spacing(2),
      paddingRight: theme.spacing(2),
      height: theme.spacing(5),
      alignItems: 'center',
      backgroundColor: theme.palette.background.three,
      cursor: 'pointer',
    },
    title: {
      ...theme.typography.subtitle2,
      color: theme.palette.foreground.one,
      marginRight: 'auto',
    },
  });
});

const DataDrawerToggle = (props: DataDrawerToggleProps) => {
  const { opened, toggle } = props;
  const classes = useStyles();
  return (
    <div className={classes.root} onClick={toggle}>
      <div className={classes.title}>Underlying Data</div>
      {opened ? <ExpandMoreIcon /> : <ExpandLessIcon />}
    </div>
  );
};

export default DataDrawerToggle;
