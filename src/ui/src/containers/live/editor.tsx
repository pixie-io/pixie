import * as ls from 'common/localstorage';
import {CodeEditor} from 'components/code-editor';
import LazyPanel from 'components/lazy-panel';
import {parseSpecs} from 'components/vega/spec';
import * as React from 'react';
import {debounce} from 'utils/debounce';

import IconButton from '@material-ui/core/IconButton';
import {createStyles, makeStyles, Theme, withStyles} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import CloseIcon from '@material-ui/icons/Close';

import {
    LiveContext, PlacementContextOld, ScriptContext, VegaContextOld, VisContext,
} from './context';
import {parsePlacementOld} from './layout';
import {parseVis} from './vis';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      color: theme.palette.foreground.one,
    },
    tabs: {
      display: 'flex',
      flexDirection: 'row',
      backgroundColor: theme.palette.background.three,
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
  const { updateVegaSpecOld } = React.useContext(LiveContext);
  const spec = React.useContext(VegaContextOld);
  const [code, setCode] = React.useState('');
  const updateVegaSpecDebounce = React.useMemo(() => debounce(updateVegaSpecOld, 2000), []);

  React.useEffect(() => {
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

const VisEditor = () => {
  const classes = useStyles();
  const { updateVis } = React.useContext(LiveContext);
  const vis = React.useContext(VisContext);
  const [code, setCode] = React.useState('');
  const updateVisDebounce = React.useMemo(() => debounce(updateVis, 2000), []);

  React.useEffect(() => {
    const newVis = parseVis(code);
    if (newVis) {
      updateVisDebounce(newVis);
    }
  }, [code]);

  React.useEffect(() => {
    setCode(JSON.stringify(vis, null, 2));
  }, [vis]);

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
  const { updatePlacementOld } = React.useContext(LiveContext);
  const placement = React.useContext(PlacementContextOld);
  const [code, setCode] = React.useState('');
  const updatePlacementDebounce = React.useMemo(() => debounce(updatePlacementOld, 2000), []);

  React.useEffect(() => {
    const newPlacement = parsePlacementOld(code);
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
    root: {
      flex: 1,
    },
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

interface LiveViewEditorProps {
  onClose: () => void;
}

const LiveViewEditor = (props: LiveViewEditorProps) => {
  const { oldLiveViewMode } = React.useContext(LiveContext);
  if (oldLiveViewMode) {
    return <LiveViewEditorOld {...props} />;
  }

  const classes = useStyles();
  const [tab, setTab] = React.useState('pixie');

  return (
    <div className={classes.root}>
      <div className={classes.tabs}>
        <StyledTabs
          value={tab}
          onChange={(event, newTab) => setTab(newTab)}
        >
          <StyledTab value='pixie' label='PXL Script' />
          <StyledTab value='vis' label='Vis Spec' />
        </StyledTabs>
        <IconButton onClick={props.onClose}>
          <CloseIcon />
        </IconButton>
      </div>
      <LazyPanel className={classes.panel} show={tab === 'pixie'}>
        <ScriptEditor />
      </LazyPanel>
      <LazyPanel className={classes.panel} show={tab === 'vis'}>
        <VisEditor />
      </LazyPanel>
    </div>
  );
};

const LiveViewEditorOld = (props: LiveViewEditorProps) => {
  const classes = useStyles();

  const [tab, setTab] = React.useState('pixie');

  return (
    <div className={classes.root}>
      <div className={classes.tabs}>
        <StyledTabs
          value={tab}
          onChange={(event, newTab) => setTab(newTab)}
        >
          <StyledTab value='pixie' label='PXL Script' />
          <StyledTab value='vega' label='Viz Spec' />
          <StyledTab value='placement' label='Placement' />
        </StyledTabs>
        <IconButton onClick={props.onClose}>
          <CloseIcon />
        </IconButton>
      </div>
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
