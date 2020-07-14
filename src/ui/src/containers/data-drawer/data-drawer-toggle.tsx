import PixieLogo from 'components/icons/pixie-logo';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';

import {
  createStyles, fade, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';

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
    paddingBottom: theme.spacing(1),
  },
  icon: {
    marginLeft: theme.spacing(2),
    marginRight: theme.spacing(2),
  },
  pixieLogo: {
    width: '48px',
    marginLeft: theme.spacing(1),
    alignSelf: 'center',
    marginRight: theme.spacing(2),
    fill: theme.palette.primary.main,
  },
  label: {
    paddingLeft: theme.spacing(2),
    paddingRight: theme.spacing(2),
  },
  dataTabLabel: {
    color: theme.palette.secondary.light,
    minWidth: 0,
    paddingLeft: `${theme.spacing(1)}px !important`,
    paddingRight: `${theme.spacing(1)}px !important`,
    '&:focus': {
      color: `${theme.palette.secondary.main} !important`,
    },
    '&:after': {
      content: '""',
      background: theme.palette.foreground.grey2,
      position: 'absolute',
      height: '75%',
      width: theme.spacing(0.2),
      right: 0,
    },
  },
  statsTabLabel: {
    color: theme.palette.secondary.main,
    '&:focus': {
      color: `${theme.palette.secondary.dark} !important`,
    },
  },
  spacer: {
    flex: 1,
  },
}));

const StyledTabs = withStyles((theme: Theme) => createStyles({
  root: {
    flex: 1,
    minHeight: theme.spacing(4),
  },
  indicator: {
    backgroundColor: theme.palette.foreground.one,
  },
}))(Tabs);

const StyledTab = withStyles((theme: Theme) => createStyles({
  root: {
    minHeight: theme.spacing(4),
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

  const { stats, tables } = React.useContext(ResultsContext);

  const tabs = React.useMemo(() => Object.keys(tables).map((tableName) => ({
    title: tableName,
  })), [tables]);

  React.useEffect(() => {
    if (tabs.length > 0 && activeTab === '') {
      setActiveTab(tabs[0].title);
    }
  }, [tabs, setActiveTab, activeTab]);

  const handleClick = React.useCallback((event) => {
    if (event.target.className.baseVal.includes('SvgIcon')) {
      // Clicking the scroll icon should not trigger the drawer to open/close.
      event.stopPropagation();
    }
  }, []);

  return (
    <div className={classes.root} onClick={toggle}>
      <span className={classes.label}>Underlying Data:</span>
      <StyledTabs
        value={activeTab}
        onChange={onTabChange}
        variant='scrollable'
        scrollButtons='auto'
        onClick={handleClick}
      >
        {tabs.map((tab) => (
          <StyledTab
            key={tab.title}
            className={classes.dataTabLabel}
            value={tab.title}
            label={tab.title}
          />
        ))}
        <div className={classes.spacer} />
        {stats ? <StyledTab className={classes.statsTabLabel} value='stats' label='Execution Stats' /> : null}
      </StyledTabs>
      <PixieLogo className={classes.pixieLogo} />
    </div>
  );
};

export default DataDrawerToggle;
