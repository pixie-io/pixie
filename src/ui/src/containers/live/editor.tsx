import * as ls from 'common/localstorage';
import {CodeEditor} from 'components/code-editor';
import LazyPanel from 'components/lazy-panel';
import {parseSpecs} from 'components/vega/spec';
import * as React from 'react';
import {debounce} from 'utils/debounce';

import {createStyles, makeStyles, Theme, withStyles} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';

import {LiveContext, PlacementContext, ScriptContext, VegaContext} from './context';
import {parsePlacement} from './layout';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      color: theme.palette.foreground.one,
    },
    panel: {
      flex: 1,
      minHeight: 0,
    },
    editor: {
      height: '100%',
      '&.pl-code-editor .CodeMirror, & .CodeMirror-scrollbar-filler': {
        backgroundColor: theme.palette.background.default,
      },
      '&.pl-code-editor .CodeMirror .CodeMirror-linenumber': {
        paddingRight: theme.spacing(1.5),
      },
    },
  }));

const VegaSpecEditor = () => {
  const classes = useStyles();
  const { updateVegaSpec } = React.useContext(LiveContext);
  const spec = React.useContext(VegaContext);
  const [code, setCode] = React.useState('');
  const updateVegaSpecDebounce = React.useMemo(() => debounce(updateVegaSpec, 2000), []);

  React.useEffect(() => {
    ls.setLiveViewVegaSpec(code);
    const specs = parseSpecs(code);
    if (specs) {
      updateVegaSpecDebounce(specs);
    }
  }, [code]);

  React.useEffect(() => {
    setCode(JSON.stringify(spec, null, 2));
  }, [spec]);

  return (
    <CodeEditor
      className={classes.editor}
      code={code}
      onChange={setCode}
    />
  );
};

const ScriptEditor = () => {
  const classes = useStyles();
  const { updateScript } = React.useContext(LiveContext);
  const code = React.useContext(ScriptContext);

  return (
    <CodeEditor
      className={classes.editor}
      code={code}
      onChange={updateScript}
    />
  );
};

const PlacementEditor = () => {
  const classes = useStyles();
  const { updatePlacement } = React.useContext(LiveContext);
  const placement = React.useContext(PlacementContext);
  const [code, setCode] = React.useState('');
  const updatePlacementDebounce = React.useMemo(() => debounce(updatePlacement, 2000), []);

  React.useEffect(() => {
    ls.setLiveViewPlacementSpec(code);
    const newPlacement = parsePlacement(code);
    if (newPlacement) {
      updatePlacementDebounce(newPlacement);
    }
  }, [code]);

  React.useEffect(() => {
    setCode(JSON.stringify(placement, null, 2));
  }, [placement]);

  return (
    <CodeEditor
      className={classes.editor}
      code={code}
      onChange={setCode}
    />
  );
};

const StyledTabs = withStyles((theme: Theme) =>
  createStyles({
    indicator: {
      backgroundColor: theme.palette.foreground.one,
    },
  }),
)(Tabs);

const StyledTab = withStyles((theme: Theme) =>
  createStyles({
    root: {
      textTransform: 'none',
      '&:focus': {
        color: theme.palette.foreground.two,
      },
    },
  }),
)(Tab);

const LiveViewEditor = () => {
  const classes = useStyles();

  const [tab, setTab] = React.useState('pixie');

  return (
    <div className={classes.root}>
      <StyledTabs
        value={tab}
        onChange={(event, newTab) => setTab(newTab)}
      >
        <StyledTab value='pixie' label='PXL Script' />
        <StyledTab value='vega' label='Viz Spec' />
        <StyledTab value='placement' label='Placement' />
      </StyledTabs>
      <LazyPanel className={classes.panel} show={tab === 'pixie'}>
        <ScriptEditor />
      </LazyPanel>
      <LazyPanel className={classes.panel} show={tab === 'vega'}>
        <VegaSpecEditor />
      </LazyPanel>
      <LazyPanel className={classes.panel} show={tab === 'placement'}>
        <PlacementEditor />
      </LazyPanel>
    </div>
  );
};

export default LiveViewEditor;
