import LazyPanel from 'components/lazy-panel';
import { VizierDataTableWithDetails } from 'components/vizier-data-table/vizier-data-table';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';

import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';

const DataViewer = () => {
  const { tables } = React.useContext(ResultsContext);
  const tabs = React.useMemo(() => Object.keys(tables).map((tableName) => ({
    title: tableName,
    content: <VizierDataTableWithDetails table={tables[tableName]} />,
  })), [tables]);

  if (tabs.length === 0) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'row',
        height: '100%',
        alignItems: 'center',
        justifyContent: 'center',
      }}
      >
        No data available to show
      </div>
    );
  }
  return <DataViewerTabs tabs={tabs} />;
};

const StyledTabs = withStyles((theme: Theme) => createStyles({
  root: {
    minHeight: theme.spacing(2),
    borderRight: `solid 1px ${theme.palette.background.three}`,
  },
  indicator: {
    backgroundColor: theme.palette.foreground.one,
  },
  scrollButtons: {
    height: theme.spacing(3),
  },
}))(Tabs);

const StyledTab = withStyles((theme: Theme) => createStyles({
  root: {
    minHeight: theme.spacing(2),
    textTransform: 'none',
    '&:focus': {
      color: theme.palette.foreground.two,
    },
  },
}))(Tab);

const useStyles = makeStyles(() => createStyles({
  root: {
    display: 'flex',
    flexDirection: 'row',
    height: '100%',
  },
  panel: {
    flex: 1,
  },
}));

interface DataViewerTabsProps {
  tabs: Array<{ title: string; content: React.ReactNode }>;
}

const DataViewerTabs = (props: DataViewerTabsProps) => {
  const { tabs } = props;
  const [activeTab, setActiveTab] = React.useState(0);
  React.useEffect(() => {
    setActiveTab(0);
  }, [tabs]);

  const classes = useStyles();
  return (
    <div className={classes.root}>
      <StyledTabs
        value={activeTab}
        orientation='vertical'
        variant='scrollable'
        scrollButtons='auto'
        onChange={(event, newTab) => setActiveTab(newTab)}
      >
        {
          tabs.map(({ title }, i) => (
            <StyledTab value={i} key={title} label={title} />
          ))
        }
      </StyledTabs>
      {
        tabs.map((tab, i) => (
          <LazyPanel
            show={activeTab === i}
            key={tab.title}
            className={classes.panel}
          >
            {tab.content}
          </LazyPanel>
        ))
      }
    </div>
  );
};

export default DataViewer;
