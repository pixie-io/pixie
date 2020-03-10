import * as ls from 'common/localstorage';
import {CodeEditor} from 'components/code-editor';
import LazyPanel from 'components/lazy-panel';
import {parseSpecs} from 'components/vega/spec';
import * as React from 'react';

import FormControl from '@material-ui/core/FormControl';
import IconButton from '@material-ui/core/IconButton';
import InputLabel from '@material-ui/core/InputLabel';
import MenuItem from '@material-ui/core/MenuItem';
import Select from '@material-ui/core/Select';
import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import ReplayIcon from '@material-ui/icons/Replay';

import {LiveContext, PlacementContext, ScriptContext, VegaContext} from './context';
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
    form: {
      margin: theme.spacing(1),
    },
    formLabel: {
      marginLeft: theme.spacing(1),
    },
  }));

const VegaSpecEditor = () => {
  const classes = useStyles();
  const { updateVegaSpec } = React.useContext(LiveContext);
  const spec = React.useContext(VegaContext);
  const [code, setCode] = React.useState('');

  React.useEffect(() => {
    const specs = parseSpecs(code);
    if (specs) {
      updateVegaSpec(specs);
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

  React.useEffect(() => {
    ls.setLiveViewPlacementSpec(code);
    const newPlacement = parsePlacement(code);
    if (newPlacement) {
      updatePlacement(placement);
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

  const { exampleScripts, setScripts } = React.useContext(LiveContext);
  const liveScripts = [];
  const liveScriptMap = {};
  exampleScripts.forEach((s) => {
    if (s.vis && s.placement) {
      liveScripts.push(s);
      liveScriptMap[s.title] = s;
    }
  });
  const selectScript = (e) => {
    const s = liveScriptMap[e.target.value];

    setScripts(s.code, s.vis, s.placement);
  };

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
      <FormControl className={classes.form}>
        <InputLabel className={classes.formLabel} id='preset-script'>Example Scripts</InputLabel>
        <Select
          labelId='preset-script'
          onChange={selectScript}
          value={''}
          >
          {
            liveScripts.map((s) => {
              return <MenuItem value={s.title} key={s.title}>{s.title}</MenuItem>;
            })
          }
        </Select>
      </FormControl>
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
