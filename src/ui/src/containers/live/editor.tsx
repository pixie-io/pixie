import {CodeEditor} from 'components/code-editor';
import {SplitContainer, SplitPane} from 'components/split-pane/split-pane';
import * as React from 'react';

import {createStyles, makeStyles, Theme, ThemeProvider} from '@material-ui/core/styles';

import {LiveContext, ScriptContext, VegaContext} from './context';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
    },
    editor: {
      height: '100%',
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

const LiveViewEditor = () => {
  const classes = useStyles();
  return (
    <SplitContainer className={classes.root} initialSizes={[50, 50]}>
      <SplitPane title='Script Editor' id='script'>
        <ScriptEditor />
      </SplitPane>
      <SplitPane title='Vega Editor' id='vega'>
        <VegaSpecEditor />
      </SplitPane>
    </SplitContainer>
  );
};

export default LiveViewEditor;
