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

import * as React from 'react';

import {
  KeyboardArrowDown as DownIcon,
  KeyboardArrowUp as UpIcon,
} from '@mui/icons-material';
import { alpha, Fab, Tab, Tabs } from '@mui/material';
import { Theme, styled } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { LazyPanel, ResizableDrawer, Spinner } from 'app/components';
import { MinimalLiveDataTable } from 'app/containers/live-data-table/live-data-table';
import { DataDrawerContext } from 'app/context/data-drawer-context';
import { LayoutContext } from 'app/context/layout-context';
import { ResultsContext } from 'app/context/results-context';

import ExecutionStats from './execution-stats';

export const STATS_TAB_NAME = 'stats';

const useStyles = makeStyles((theme: Theme) => createStyles({
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
  tabBody: {
    marginTop: theme.spacing(2),
    flex: 1,
    display: 'flex',
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
    minWidth: 0,
    paddingLeft: `${theme.spacing(1.4)} !important`,
    paddingRight: `${theme.spacing(1.4)} !important`,
    paddingTop: `${theme.spacing(0.7)} !important`,
    paddingBottom: `${theme.spacing(0.7)} !important`,
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  statsTabLabel: {
    paddingRight: `${theme.spacing(2)} !important`,
  },
  toggle: {
    display: 'flex',
    justifyContent: 'center',
    pointerEvents: 'auto',
  },
  fab: {
    height: theme.spacing(2),
    zIndex: theme.zIndex.drawer - 1, // Let overflow hide behind the drawer
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
}), { name: 'DataDrawer' });

const TabSpacer = React.memo(() => <div className={useStyles().spacer} />);
TabSpacer.displayName = 'TabSpacer';

// eslint-disable-next-line react-memo/require-memo
const StyledTabs = styled(Tabs)(({ theme }) => ({
  minHeight: theme.spacing(4),
  '& .MuiTabs-indicator': {
    backgroundColor: theme.palette.foreground.one,
  },
}));

// eslint-disable-next-line react-memo/require-memo
const StyledTab = styled(Tab)(({ theme }) => ({
  minHeight: theme.spacing(4),
  padding: 0,
  textTransform: 'none',
  '&:focus': {
    color: theme.palette.foreground.two,
  },
  color: alpha(theme.palette.primary.dark, 0.5),
  ...theme.typography.subtitle1,
  fontWeight: 400,
  maxWidth: theme.spacing(37.5), // 300px
}));

const DataDrawer = React.memo<{ open: boolean }>(({ open }) => {
  const classes = useStyles();
  const { loading, stats, tables } = React.useContext(ResultsContext);
  const { activeTab, setActiveTab } = React.useContext(DataDrawerContext);

  const onTabChange = React.useCallback((event, newTab) => {
    setActiveTab(newTab);
    if (open && newTab !== activeTab) {
      event.stopPropagation();
    }
  }, [activeTab, setActiveTab, open]);

  const tabs = React.useMemo(() => Array.from(tables.entries()).map(([tableName, table]) => ({
    title: tableName,
    content: <MinimalLiveDataTable table={table} />,
  })), [tables]);

  // If the selected table is not in the new result set, show the first table.
  React.useEffect(() => {
    if (open && tabs.length && activeTab !== STATS_TAB_NAME) {
      const selectedTable = tabs.find((t) => t.title === activeTab);
      if (!selectedTable) {
        setActiveTab(tabs[0].title);
      }
    }
  }, [open, tabs, activeTab, setActiveTab]);

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
        value={activeTabExists ? activeTab : false}
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
        <TabSpacer />
        {
          stats ? (
            <StyledTab
              className={`${classes.statsTabLabel} ${STATS_TAB_NAME !== activeTab ? '' : classes.selectedTabLabel}`}
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
                    className={classes.tabBody}
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
});
DataDrawer.displayName = 'DataDrawer';

const DrawerToggleButton = React.memo(() => {
  const classes = useStyles();
  const { dataDrawerOpen, setDataDrawerOpen } = React.useContext(LayoutContext);

  const toggleDrawerOpen = React.useCallback(() => setDataDrawerOpen((open) => !open), [setDataDrawerOpen]);

  return (
    <div className={classes.otherContent}>
      <div className={classes.spacer} />
      <div className={classes.toggle}>
        <Fab className={classes.fab} onClick={toggleDrawerOpen} variant='extended' size='medium'>
          { dataDrawerOpen ? <DownIcon /> : <UpIcon /> }
        </Fab>
      </div>
    </div>
  );
});
DrawerToggleButton.displayName = 'DrawerToggleButton';

export const DataDrawerSplitPanel = React.memo(() => {
  const { dataDrawerOpen } = React.useContext(LayoutContext);

  return (
    <ResizableDrawer
      drawerDirection='bottom'
      initialSize={350}
      minSize={0}
      open={dataDrawerOpen}
      otherContent={React.useMemo(() => <DrawerToggleButton />, [])}
      overlay={false}
    >
      <DataDrawer open={dataDrawerOpen} />
    </ResizableDrawer>
  );
});
DataDrawerSplitPanel.displayName = 'DataDrawerSplitPanel';
