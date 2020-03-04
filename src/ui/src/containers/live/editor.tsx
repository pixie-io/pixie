import {CodeEditor} from 'components/code-editor';
import LazyPanel from 'components/lazy-panel';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';

import {LiveContext, ScriptContext, VegaContext} from './context';

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
  const code = React.useContext(VegaContext);

  return (
    <CodeEditor
      className={classes.editor}
      code={code}
      onChange={updateVegaSpec}
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
  return <div>coming soon</div>;
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
