import LazyPanel from 'components/lazy-panel';
import { Spinner } from 'components/spinner/spinner';
import { DataDrawerContext } from 'context/data-drawer-context';
import { LayoutContext } from 'context/layout-context';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';
import ResizableDrawer from 'components/drawer/resizable-drawer';
import { VizierDataTableWithDetails } from 'components/vizier-data-table/vizier-data-table';

import {
  createStyles, makeStyles, Theme,
} from '@material-ui/core/styles';

import DataDrawerToggle from './data-drawer-toggle';
import ExecutionStats from './execution-stats';

const useStyles = makeStyles((theme: Theme) => createStyles({
  splits: {
    '& .gutter': {
      backgroundColor: theme.palette.background.three,
    },
  },
  drawerRoot: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    backgroundColor: theme.palette.background.default,
  },
  content: {
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
    display: 'flex',
    flexDirection: 'column',
    height: '100%',
    width: '100%',
  },
}));

const DataDrawer = ({ open, activeTab, setActiveTab }) => {
  const classes = useStyles();
  const { loading, tables } = React.useContext(ResultsContext);

  const tabs = React.useMemo(() => Object.keys(tables).map((tableName) => ({
    title: tableName,
    content: <VizierDataTableWithDetails table={tables[tableName]} />,
  })), [tables]);

  // If the selected table is not in the new result set, show the first table.
  if (open && tabs.length) {
    const selectedTable = tabs.find((t) => t.title === activeTab);
    if (!selectedTable) {
      setActiveTab(tabs[0].title);
    }
  }

  return (
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
                  className={classes.content}
                  show={open && activeTab === tab.title}
                >
                  {tab.content}
                </LazyPanel>
              ))
            }
            <LazyPanel className={classes.content} show={open && activeTab === 'stats'}>
              <ExecutionStats />
            </LazyPanel>
          </>
        )
      }
    </div>
  );
};

export const DataDrawerSplitPanel = (props) => {
  const classes = useStyles();

  const { dataDrawerOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const { activeTab, setActiveTab } = React.useContext(DataDrawerContext);

  const toggleDrawerOpen = () => setDataDrawerOpen((open) => !open);

  const contents = (
    <div className={classes.otherContent}>
      {props.children}
      <DataDrawerToggle
        opened={dataDrawerOpen}
        toggle={toggleDrawerOpen}
        activeTab={activeTab}
        setActiveTab={setActiveTab}
      />
    </div>
  );

  return (
    <ResizableDrawer
      drawerDirection='bottom'
      initialSize={350}
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
