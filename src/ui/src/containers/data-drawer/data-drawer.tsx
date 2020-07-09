import LazyPanel from 'components/lazy-panel';
import { Spinner } from 'components/spinner/spinner';
import { DataDrawerContext, DataDrawerTabsKey } from 'context/data-drawer-context';
import { LayoutContext } from 'context/layout-context';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';
import ResizableDrawer from 'components/drawer/resizable-drawer';

import {
  createStyles, makeStyles, Theme,
} from '@material-ui/core/styles';

import DataDrawerToggle from './data-drawer-toggle';
import DataViewer from './data-viewer';
import ErrorPanel from './error-panel';
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

const DataDrawer = (props) => {
  const classes = useStyles();
  const { loading } = React.useContext(ResultsContext);

  return (
    <div className={classes.drawerRoot}>
      {
        loading
          ? (props.open ? <div className={classes.spinner}><Spinner /></div> : null)
          : (
            <>
              <LazyPanel className={classes.content} show={props.open && props.activeTab === 'data'}>
                <DataViewer />
              </LazyPanel>
              <LazyPanel className={classes.content} show={props.open && props.activeTab === 'errors'}>
                <ErrorPanel />
              </LazyPanel>
              <LazyPanel className={classes.content} show={props.open && props.activeTab === 'stats'}>
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
        setActiveTab={(tab: DataDrawerTabsKey) => setActiveTab(tab)}
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
      />
    </ResizableDrawer>
  );
};
