import {CodeEditor} from 'components/code-editor';
import * as React from 'react';
import Split from 'react-split';

import {createStyles, makeStyles, Theme, ThemeProvider} from '@material-ui/core/styles';

import {LiveContext, ScriptContext, VegaContext} from './context';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
    },
  }));

const VegaSpecEditor = () => {
  const { updateVegaSpec } = React.useContext(LiveContext);
  const code = React.useContext(VegaContext);

  return (
    <CodeEditor
      code={code}
      onChange={updateVegaSpec}
    />
  );
};

const ScriptEditor = () => {
  const { updateScript } = React.useContext(LiveContext);
  const code = React.useContext(ScriptContext);

  return (
    <CodeEditor
      code={code}
      onChange={updateScript}
    />
  );
};

const LiveViewEditor = () => {
  const classes = useStyles();
  return (
    <Split className={classes.root} direction='vertical'>
      <ScriptEditor />
      <VegaSpecEditor />
    </Split>
  );
};

export default LiveViewEditor;
