import PixieLogo from 'components/icons/pixie-logo';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';

import {
  createStyles, fade, makeStyles, Theme, withStyles,
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
    height: theme.spacing(4),
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.palette.background.default,
    cursor: 'pointer',
    boxShadow: `inset 0 ${theme.spacing(0.3)}px ${theme.spacing(1)}px ${fade(theme.palette.foreground.grey5, 0.1)}`,
    paddingTop: theme.spacing(1),
  },
  icon: {
    marginLeft: theme.spacing(2),
    marginRight: theme.spacing(2),
  },
  pixieLogo: {
    width: '48px',
    marginLeft: 'auto',
    alignSelf: 'center',
    marginRight: theme.spacing(2),
    fill: theme.palette.primary.main,
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
    ...theme.typography.body2,
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
  }, [activeTab, setActiveTab, error, stats]);

  return (
    <div className={classes.root} onClick={toggle}>
      <StyledTabs value={activeTab} onChange={onTabChange}>
        <StyledTab value='data' label='Underlying Data' />
        {stats ? <StyledTab value='stats' label='Execution Stats' /> : null}
      </StyledTabs>
      <PixieLogo className={classes.pixieLogo} />
    </div>
  );
};

export default DataDrawerToggle;
