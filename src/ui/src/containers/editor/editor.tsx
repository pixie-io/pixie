import clsx from 'clsx';
import { CodeEditor } from 'components/code-editor';
import LazyPanel from 'components/lazy-panel';
import * as React from 'react';
import Split from 'react-split';
import { triggerResize } from 'utils/resize';

import {
  createStyles, makeStyles, Theme, useTheme, withStyles,
} from '@material-ui/core/styles';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import ChevronLeft from '@material-ui/icons/ChevronLeft';

import { LayoutContext } from 'context/layout-context';
import { ScriptContext } from 'context/script-context';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    color: theme.palette.foreground.one,
    minWidth: 0,
    overflow: 'hidden',
  },
  rootPanel: {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
  },
  splits: {
    '& .gutter:hover': {
      cursor: 'col-resize',
    },
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
  },
  closer: {
    cursor: 'pointer',
    display: 'flex',
    alignItems: 'center',
  },
}));

const VisEditor = () => {
  const classes = useStyles();
  const { visJSON, setVisEditorText } = React.useContext(ScriptContext);

  const editorRef = React.createRef<CodeEditor>();
  // We useEffect instead of relying on the prop because of an issue where a cursor
  // in the field causes onChange to be triggered partway through, leading to a
  // partial state being set.
  React.useEffect(() => {
    if (!editorRef.current) {
      return;
    }

    editorRef.current.changeEditorValue(visJSON);
  }, [visJSON]);

  return (
    <CodeEditor
      ref={editorRef}
      onChange={(code: string) => {
        setVisEditorText(code);
      }}
      className={classes.editor}
      language='json'
    />
  );
};

const ScriptEditor = () => {
  const classes = useStyles();
  const { pxl, setPxlEditorText } = React.useContext(ScriptContext);
  const editorRef = React.createRef<CodeEditor>();
  // We useEffect instead of relying on the prop because of an issue where a cursor
  // in the field causes onChange to be triggered partway through, leading to a
  // partial state being set.
  React.useEffect(() => {
    if (!editorRef.current) {
      return;
    }
    editorRef.current.changeEditorValue(pxl);
  }, [pxl]);

  return (
    <CodeEditor
      ref={editorRef}
      onChange={(code: string) => {
        setPxlEditorText(code);
      }}
      className={classes.editor}
      language='python'
    />
  );
};

const StyledTabs = withStyles((theme: Theme) => createStyles({
  root: {
    flex: 1,
  },
  indicator: {
    backgroundColor: theme.palette.foreground.one,
  },
}))(Tabs);

const StyledTab = withStyles((theme: Theme) => createStyles({
  root: {
    textTransform: 'none',
    '&:focus': {
      color: theme.palette.foreground.two,
    },
  },
}))(Tab);

const LiveViewEditor = () => {
  const classes = useStyles();
  const [tab, setTab] = React.useState('pixie');
  const { setEditorPanelOpen, editorPanelOpen } = React.useContext(LayoutContext);
  const closeEditor = () => setEditorPanelOpen(false);

  return (
    <div className={classes.root}>
      <LazyPanel show={editorPanelOpen} className={classes.rootPanel}>
        <div className={classes.tabs}>
          <StyledTabs
            value={tab}
            onChange={(event, newTab) => setTab(newTab)}
          >
            <StyledTab value='pixie' label='PXL Script' />
            <StyledTab value='vis' label='Vis Spec' />
          </StyledTabs>
          <div className={classes.closer} onClick={closeEditor}>
            <ChevronLeft />
          </div>
        </div>
        <LazyPanel className={classes.panel} show={tab === 'pixie'}>
          <ScriptEditor />
        </LazyPanel>
        <LazyPanel className={classes.panel} show={tab === 'vis'}>
          <VisEditor />
        </LazyPanel>
      </LazyPanel>
    </div>
  );
};

export const EditorSplitPanel = (props) => {
  const ref = React.useRef(null);
  const theme = useTheme();
  const classes = useStyles();
  const {
    editorPanelOpen,
    editorSplitsSizes,
    setEditorPanelOpen,
    setEditorSplitSizes,
  } = React.useContext(LayoutContext);

  const [collapsedPanel, setCollapsedPanel] = React.useState<null | 0>(null);

  const dragHandler = ((sizes) => {
    if (sizes[0] <= 5) { // Snap the editor close when it is less than 5%.
      setEditorPanelOpen(false);
    } else {
      setEditorPanelOpen(true);
      setEditorSplitSizes(sizes);
    }
  });

  React.useEffect(() => {
    if (!editorPanelOpen) {
      setCollapsedPanel(0);
    } else {
      setCollapsedPanel(null);
      ref.current.split.setSizes(editorSplitsSizes);
    }
    triggerResize();
  }, [editorPanelOpen, editorSplitsSizes]);

  return (
    <Split
      ref={ref}
      direction='horizontal'
      sizes={editorSplitsSizes}
      className={clsx(props.className, classes.splits)}
      gutterSize={theme.spacing(0.5)}
      minSize={0}
      onDragEnd={dragHandler}
      collapsed={collapsedPanel}
      cursor='col-resize'
    >
      <LiveViewEditor />
      {props.children}
    </Split>
  );
};
