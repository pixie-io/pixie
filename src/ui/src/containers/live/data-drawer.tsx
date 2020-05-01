import clsx from 'clsx';
import LazyPanel from 'components/lazy-panel';
import * as React from 'react';
import Split from 'react-split';

import { createStyles, makeStyles, Theme, useTheme } from '@material-ui/core/styles';

import { LayoutContext } from './context/layout-context';
import DataDrawerToggle from './data-drawer-toggle';
import DataViewer from './data-viewer';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    splits: {
      '& .gutter': {
        backgroundColor: theme.palette.background.three,
      },
    },
    drawerRoot: {
      display: 'flex',
      flexDirection: 'column',
    },
    dataViewer: {
      flex: 1,
      minHeight: 0,
    },
  });
});

const DataDrawer = () => {
  const { dataDrawerOpen, setDataDrawerOpen } = React.useContext(LayoutContext);
  const toggleDrawerOpen = () => setDataDrawerOpen((open) => !open);
  const classes = useStyles();

  return (
    <div className={classes.drawerRoot}>
      <DataDrawerToggle opened={dataDrawerOpen} toggle={toggleDrawerOpen} />
      <LazyPanel className={classes.dataViewer} show={dataDrawerOpen}>
        <DataViewer />
      </LazyPanel>
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
