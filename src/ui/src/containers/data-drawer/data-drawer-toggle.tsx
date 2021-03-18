import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import { PixieLogo } from '@pixie-labs/components';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';

import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';

interface DataDrawerToggleProps {
  opened: boolean;
  activeTab: string;
  setActiveTab: (newTab: string) => void;
  toggle: () => void;
}

export const STATS_TAB_NAME = 'stats';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.palette.sideBar.color,
    borderTop: theme.palette.border.unFocused,
    cursor: 'pointer',
    boxShadow: '0px -4px 4px #00000042',
    zIndex: 100,
  },
  toggleIcon: {
    marginLeft: theme.spacing(1),
    marginRight: theme.spacing(1),
  },
  pixieLogo: {
    width: '48px',
    marginLeft: theme.spacing(1),
    alignSelf: 'center',
    marginRight: theme.spacing(2),
    fill: theme.palette.foreground.three,
  },
  label: {
    paddingLeft: theme.spacing(2),
    paddingRight: theme.spacing(2),
  },
  selectedTabLabel: {
    color: `${theme.palette.primary.light} !important`,
  },
  dataTabLabel: {
    '&:after': {
      content: '""',
      background: theme.palette.foreground.three,
      opacity: 0.4,
      position: 'absolute',
      height: theme.spacing(2.6),
      width: theme.spacing(0.25),
      right: 0,
    },
    opacity: 1,
    color: theme.palette.foreground.three,
    minWidth: 0,
    paddingLeft: `${theme.spacing(1.4)}px !important`,
    paddingRight: `${theme.spacing(1.4)}px !important`,
    paddingTop: `${theme.spacing(0.7)}px !important`,
    paddingBottom: `${theme.spacing(0.7)}px !important`,
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  statsTabLabel: {
    '&:focus': {
      color: `${theme.palette.primary.light} !important`,
    },
    color: theme.palette.foreground.three,
  },
  spacer: {
    flex: 1,
  },
  emptyLabel: {
    display: 'none',
  },
}));

const TabSpacer = (props) => (<div className={props.classes.spacer} />);

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
    color: `${theme.palette.primary.dark}80`, // Make text darker by lowering opacity to 50%.
    ...theme.typography.subtitle1,
    fontWeight: 400,
    maxWidth: 300,
  },
  wrapper: {
    alignItems: 'flex-start',
  },
}))(Tab);

export const DataDrawerToggle = (props: DataDrawerToggleProps) => {
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
    if (event.target.className.baseVal?.includes('SvgIcon')) {
      // Clicking the scroll icon should not trigger the drawer to open/close.
      event.stopPropagation();
    }
  }, []);

  const activeTabExists = activeTab && tabs.some((tab) => tab.title === activeTab);

  return (
    <div className={classes.root} onClick={toggle}>
      {
        opened ? <DownIcon className={classes.toggleIcon} /> : <UpIcon className={classes.toggleIcon} />
      }
      <StyledTabs
        value={activeTabExists ? activeTab : ''}
        onChange={onTabChange}
        variant='scrollable'
        scrollButtons='auto'
        onClick={handleClick}
      >
        <StyledTab className={classes.emptyLabel} value='' />
        {tabs.map((tab) => (
          <StyledTab
            key={tab.title}
            className={`${classes.dataTabLabel} ${tab.title !== activeTab ? '' : classes.selectedTabLabel}`}
            value={tab.title}
            label={tab.title}
          />
        ))}
        <TabSpacer classes={classes} />
        {
          stats ? (
            <StyledTab
              className={classes.statsTabLabel}
              value={STATS_TAB_NAME}
              label='Execution Stats'
            />
          ) : null
        }
      </StyledTabs>
      <PixieLogo className={classes.pixieLogo} />
    </div>
  );
};
