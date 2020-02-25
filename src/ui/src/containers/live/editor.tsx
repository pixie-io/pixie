import {getLiveViewEditorSplits, setLiveViewEditorSplits} from 'common/localstorage';
import {CodeEditor} from 'components/code-editor';
import {SplitContainer, SplitPane} from 'components/split-pane/split-pane';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import {LiveContext, ScriptContext, VegaContext} from './context';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      borderRightStyle: 'solid',
      borderRightColor: theme.palette.background.three,
      borderRightWidth: theme.spacing(0.25),
    },
    editor: {
      height: '100%',
      '&.pl-code-editor .CodeMirror': {
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

const LiveViewEditor = () => {
  const classes = useStyles();
  const [splits, setSplits] = React.useState<number[]>([]);
  const initialSizes = React.useMemo(() => getLiveViewEditorSplits(), []);

  React.useEffect(() => {
    if (splits.length === 2) {
      setLiveViewEditorSplits([splits[0], splits[1]]);
    }
  }, [splits]);

  return (
    <SplitContainer className={classes.root} initialSizes={initialSizes} onSizeChange={setSplits}>
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
