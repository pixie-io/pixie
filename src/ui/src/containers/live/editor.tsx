import * as ls from 'common/localstorage';
import {CodeEditor} from 'components/code-editor';
import LazyPanel from 'components/lazy-panel';
import {parseSpecs} from 'components/vega/spec';
import * as React from 'react';
import {debounce} from 'utils/debounce';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';

import {LiveContext, PlacementContext, ScriptContext, VegaContext} from './context';
import ExampleScripts from './example-scripts';
import {parsePlacement} from './layout';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
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

const LiveViewEditor = () => {
  const classes = useStyles();

  const [tab, setTab] = React.useState('pixie');

  return (
    <div className={classes.root}>
      <Tabs
        value={tab}
        indicatorColor='primary'
        textColor='primary'
        onChange={(event, newTab) => setTab(newTab)}
      >
        <Tab value='pixie' label='Pixie Script' />
        <Tab value='vega' label='Vega Spec' />
        <Tab value='placement' label='Placement' />
      </Tabs>
      <ExampleScripts />
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
