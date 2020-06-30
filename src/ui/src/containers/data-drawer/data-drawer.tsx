import clsx from 'clsx';
import LazyPanel from 'components/lazy-panel';
import { Spinner } from 'components/spinner/spinner';
import { DataDrawerContext, DataDrawerTabsKey } from 'context/data-drawer-context';
import { LayoutContext } from 'context/layout-context';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';
import Split from 'react-split';

import {
  createStyles, makeStyles, Theme, useTheme,
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
}));

const DataDrawer = () => {
  const { dataDrawerOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const toggleDrawerOpen = () => setDataDrawerOpen((open) => !open);
  const classes = useStyles();
  const { activeTab, setActiveTab } = React.useContext(DataDrawerContext);
  const { loading } = React.useContext(ResultsContext);

  return (
    <div className={classes.drawerRoot}>
      <DataDrawerToggle
        opened={dataDrawerOpen}
        toggle={toggleDrawerOpen}
        activeTab={activeTab}
        setActiveTab={(tab: DataDrawerTabsKey) => setActiveTab(tab)}
      />
      {
        loading
          ? (dataDrawerOpen ? <div className={classes.spinner}><Spinner /></div> : null)
          : (
            <>
              <LazyPanel className={classes.content} show={dataDrawerOpen && activeTab === 'data'}>
                <DataViewer />
              </LazyPanel>
              <LazyPanel className={classes.content} show={dataDrawerOpen && activeTab === 'errors'}>
                <ErrorPanel />
              </LazyPanel>
              <LazyPanel className={classes.content} show={dataDrawerOpen && activeTab === 'stats'}>
                <ExecutionStats />
              </LazyPanel>
            </>
          )
      }
    </div>
  );
};

export const DataDrawerSplitPanel = (props) => {
  const ref = React.useRef(null);
  const theme = useTheme();
  const classes = useStyles();
  const {
    dataDrawerSplitsSizes,
    setDataDrawerSplitsSizes,
    setDataDrawerOpen,
    dataDrawerOpen,
  } = React.useContext(LayoutContext);

  const [collapsedPanel, setCollapsedPanel] = React.useState<null | 1>(null);

  const dragHandler = ((sizes) => {
    if (sizes[1] <= 5) { // Snap the header close when it is less than 5%.
      setDataDrawerOpen(false);
    } else {
      setDataDrawerOpen(true);
      setDataDrawerSplitsSizes(sizes);
    }
  });

  React.useEffect(() => {
    if (!dataDrawerOpen) {
      setCollapsedPanel(1);
    } else {
      setCollapsedPanel(null);
      ref.current.split.setSizes(dataDrawerSplitsSizes);
    }
  }, [dataDrawerOpen, dataDrawerSplitsSizes]);

  return (
    <Split
      ref={ref}
      direction='vertical'
      sizes={dataDrawerSplitsSizes}
      className={clsx(classes.splits, props.className)}
      gutterSize={theme.spacing(0.5)}
      minSize={theme.spacing(5)}
      onDragEnd={dragHandler}
      collapsed={collapsedPanel}
    >
      {props.children}
      <DataDrawer />
    </Split>
  );
};
