import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import * as React from 'react';

import LazyPanel from 'components/lazy-panel';
import {DrawerContext, LiveContext} from './context';
import DataDrawerToggle from './data-drawer-toggle';
import DataViewer from './data-viewer';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    dataDrawer: {
      flex: 2,
      minHeight: 0,
    },
  });
});

const DataDrawer = () => {
  const { toggleDataDrawer } = React.useContext(LiveContext);
  const dataDrawerOpen = React.useContext(DrawerContext);
  const classes = useStyles();

  return (
    <>
      <DataDrawerToggle opened={dataDrawerOpen} toggle={toggleDataDrawer}/>
      <LazyPanel className={classes.dataDrawer} show={dataDrawerOpen}>
        <DataViewer />
      </LazyPanel>
    </>
  );
};

export default DataDrawer;
