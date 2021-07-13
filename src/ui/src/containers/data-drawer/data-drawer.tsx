/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { LazyPanel, ResizableDrawer, Spinner } from 'app/components';
import { DataDrawerContext } from 'app/context/data-drawer-context';
import { LayoutContext } from 'app/context/layout-context';
import { ResultsContext } from 'app/context/results-context';

import * as React from 'react';
import { LiveDataTableWithDetails } from 'app/containers/live-data-table/live-data-table';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import {
  makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import Fab from '@material-ui/core/Fab';
import ExecutionStats from './execution-stats';

export const STATS_TAB_NAME = 'stats';

const useStyles = makeStyles((theme: Theme) => createStyles({
  splits: {
    '& .gutter': {
    },
  },
  drawerRoot: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    pointerEvents: 'auto',
    overflow: 'hidden',
  },
  execStats: {
    flex: 1,
    minHeight: 0,
  },
  resultTable: {
    marginTop: theme.spacing(2),
    flex: 1,
    minHeight: 0,
  },
  spinner: {
    position: 'absolute',
    top: '50%',
    left: '50%',
    transform: 'translate(-50%, -50%)',
  },
  otherContent: {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
  },
  spacer: {
    flex: 1,
  },
  emptyLabel: {
    display: 'none',
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
    paddingLeft: `${theme.spacing(1.4)} !important`,
    paddingRight: `${theme.spacing(1.4)} !important`,
    paddingTop: `${theme.spacing(0.7)} !important`,
    paddingBottom: `${theme.spacing(0.7)} !important`,
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  statsTabLabel: {
    '&:focus': {
      color: `${theme.palette.primary.light} !important`,
    },
    '& span': {
      alignItems: 'flex-end',
      paddingRight: theme.spacing(2),
    },
    color: theme.palette.foreground.three,
  },
  toggle: {
    display: 'flex',
    justifyContent: 'center',
    pointerEvents: 'auto',
  },
  fab: {
    height: theme.spacing(2),
    zIndex: 100,
    borderTopLeftRadius: theme.spacing(1),
    borderTopRightRadius: theme.spacing(1),
    borderBottomLeftRadius: 0,
    borderBottomRightRadius: 0,
    backgroundColor: theme.palette.foreground.three,
  },
  drawer: {
    pointerEvents: 'auto',
    display: 'flex',
    flexDirection: 'column',
    width: '100%',
  },
}));

const TabSpacer = (props) => (<div className={props.classes.spacer} />);

const StyledTabs = withStyles((theme: Theme) => createStyles({
  root: {
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

const DataDrawer = ({ open, activeTab, setActiveTab }) => {
  const classes = useStyles();
  const { loading, stats, tables } = React.useContext(ResultsContext);

  const onTabChange = (event, newTab) => {
    setActiveTab(newTab);
    if (open && newTab !== activeTab) {
      event.stopPropagation();
    }
  };
  const tabs = React.useMemo(() => Object.keys(tables).map((tableName) => ({
    title: tableName,
    content: <LiveDataTableWithDetails table={tables[tableName]} />,
  })), [tables]);

  // If the selected table is not in the new result set, show the first table.
  if (open && tabs.length && activeTab !== STATS_TAB_NAME) {
    const selectedTable = tabs.find((t) => t.title === activeTab);
    if (!selectedTable) {
      setActiveTab(tabs[0].title);
    }
  }

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
    <div className={classes.drawer}>
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
      <div className={classes.drawerRoot}>
        {
          (loading && open) && <div className={classes.spinner}><Spinner /></div>
        }
        {
          !loading && (
            <>
              {
                tabs.map((tab) => (
                  <LazyPanel
                    key={tab.title}
                    className={classes.resultTable}
                    show={open && activeTab === tab.title}
                  >
                    {tab.content}
                  </LazyPanel>
                ))
              }
              <LazyPanel className={classes.execStats} show={open && activeTab === STATS_TAB_NAME}>
                <ExecutionStats />
              </LazyPanel>
            </>
          )
        }
      </div>
    </div>
  );
};

export const DataDrawerSplitPanel: React.FC = () => {
  const classes = useStyles();

  const { dataDrawerOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const { activeTab, setActiveTab } = React.useContext(DataDrawerContext);

  const toggleDrawerOpen = () => setDataDrawerOpen((open) => !open);

  const contents = (
    <div className={classes.otherContent}>
      <div className={classes.spacer} />
      <div className={classes.toggle}>
        <Fab className={classes.fab} onClick={toggleDrawerOpen} variant='extended' size='medium'>
          { dataDrawerOpen ? <DownIcon /> : <UpIcon /> }
        </Fab>
      </div>
    </div>
  );

  return (
    <ResizableDrawer
      drawerDirection='bottom'
      initialSize={350}
      minSize={0}
      open={dataDrawerOpen}
      otherContent={contents}
      overlay={false}
    >
      <DataDrawer
        open={dataDrawerOpen}
        activeTab={activeTab}
        setActiveTab={setActiveTab}
      />
    </ResizableDrawer>
  );
};
