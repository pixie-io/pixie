import PixieLogo from 'components/icons/pixie-logo';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';

import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import ExpandLessIcon from '@material-ui/icons/ExpandLess';
import ExpandMoreIcon from '@material-ui/icons/ExpandMore';

interface DataDrawerToggleProps {
  opened: boolean;
  activeTab: string;
  setActiveTab: (newTab: string) => void;
  toggle: () => void;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    minHeight: theme.spacing(5),
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.palette.background.three,
    cursor: 'pointer',
  },
  icon: {
    marginLeft: theme.spacing(2),
    marginRight: theme.spacing(2),
  },
  title: {
    ...theme.typography.subtitle2,
    color: theme.palette.foreground.one,
    marginLeft: theme.spacing(2),
  },
  pixieLogo: {
    opacity: 0.5,
    width: '48px',
    marginLeft: 'auto',
    alignSelf: 'center',
    marginRight: theme.spacing(2),
  },
}));

const StyledTabs = withStyles((theme: Theme) => createStyles({
  root: {
    flex: 1,
    minHeight: theme.spacing(5),
  },
  indicator: {
    backgroundColor: theme.palette.foreground.one,
  },
}))(Tabs);

const StyledTab = withStyles((theme: Theme) => createStyles({
  root: {
    minHeight: theme.spacing(5),
    padding: 0,
    textTransform: 'none',
    '&:focus': {
      color: theme.palette.foreground.two,
    },
  },
}))(Tab);

const DataDrawerToggle = (props: DataDrawerToggleProps) => {
  const {
    opened, toggle, activeTab, setActiveTab,
  } = props;
  const classes = useStyles();
  const onTabChange = (event, newTab) => {
    setActiveTab(newTab);
    if (opened && newTab !== activeTab) {
      event.stopPropagation();
    }
  };

  const { error, stats } = React.useContext(ResultsContext);

  React.useEffect(() => {
    if ((!error && activeTab === 'errors')
      || (!stats && activeTab === 'stats')) {
      setActiveTab('data');
    }
  }, [activeTab, error, stats]);

  return (
    <div className={classes.root} onClick={toggle}>
      {opened ? <ExpandMoreIcon className={classes.icon} /> : <ExpandLessIcon className={classes.icon} />}
      <StyledTabs value={activeTab} onChange={onTabChange}>
        <StyledTab value='data' label='Underlying Data' />
        {error ? <StyledTab value='errors' label='Errors' /> : null}
        {stats ? <StyledTab value='stats' label='Execution Stats' /> : null}
      </StyledTabs>
      <PixieLogo className={classes.pixieLogo} />
    </div>
  );
};

export default DataDrawerToggle;
