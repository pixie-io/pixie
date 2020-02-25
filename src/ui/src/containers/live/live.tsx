import clsx from 'clsx';
import {getLiveViewEditorOpened, setLiveViewEditorOpened} from 'common/localstorage';
import MagicIcon from 'components/icons/magic';
import * as React from 'react';
import {GlobalHotKeys} from 'react-hotkeys';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import EditIcon from '@material-ui/icons/Edit';
import MenuIcon from '@material-ui/icons/Menu';
import ShareIcon from '@material-ui/icons/Share';
import ToggleButton from '@material-ui/lab/ToggleButton';

import Canvas from './canvas';
import CommandInput from './command-input';
import LiveContextProvider from './context';
import Editor from './editor';
import ExecuteScript from './execute';

const useStyles = makeStyles((theme: Theme) => {
  const scrollbarStyles = (color: string) => ({
    borderRadius: theme.spacing(1.5),
    border: [['solid', theme.spacing(0.5), 'transparent']],
    backgroundColor: 'transparent',
    boxShadow: [['inset', 0, 0, theme.spacing(1), theme.spacing(1), color]],
  });
  return createStyles({
    root: {
      height: '100%',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: theme.palette.background.default,
      '& ::-webkit-scrollbar': {
        width: theme.spacing(2),
        height: theme.spacing(2),
      },
      '& ::-webkit-scrollbar-track': scrollbarStyles(theme.palette.background.one),
      '& ::-webkit-scrollbar-thumb': scrollbarStyles(theme.palette.foreground.one),
    },
    topBar: {
      display: 'flex',
      margin: theme.spacing(2),
      alignItems: 'center',
    },
    title: {
      marginLeft: theme.spacing(2),
      flexGrow: 1,
    },
    main: {
      flex: 1,
      minHeight: 0,
      display: 'flex',
      borderTopStyle: 'solid',
      borderTopColor: theme.palette.background.three,
      borderTopWidth: theme.spacing(0.25),
    },
    editorToggle: {
      border: 'none',
      borderRadius: '50%',
      color: theme.palette.action.active,
    },
    editor: {
      flex: 1,
      minWidth: 0,
      visibility: 'hidden',
      position: 'absolute',
      '&.opened': {
        visibility: 'visible',
        position: 'relative',
        borderRightStyle: 'solid',
        borderRightColor: theme.palette.background.three,
        borderRightWidth: theme.spacing(0.25),
      },
    },
    canvas: {
      flex: 1,
      minWidth: 0,
      margin: theme.spacing(1),
    },
  });
});

const COMMAND_KEYMAP = {
  PIXIE_COMMAND: ['Meta+k', 'Control+k'],
};

const LiveView = () => {
  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = React.useCallback(() => setDrawerOpen((opened) => !opened), []);

  const [editorOpen, setEditorOpen] = React.useState<boolean>(getLiveViewEditorOpened());
  const toggleEditor = React.useCallback(() => setEditorOpen((opened) => !opened), []);
  React.useEffect(() => {
    setLiveViewEditorOpened(editorOpen);
  }, [editorOpen]);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const classes = useStyles();

  const hotkeyHandlers = React.useMemo(() => ({
    PIXIE_COMMAND: toggleCommandOpen,
  }), []);

  return (
    <LiveContextProvider>
      <div className={classes.root}>
        <GlobalHotKeys handlers={hotkeyHandlers} keyMap={COMMAND_KEYMAP} />
        <div className={classes.topBar}>
          <IconButton onClick={toggleDrawer}>
            <MenuIcon />
          </IconButton>
          <div className={classes.title}>title goes here</div>
          <ExecuteScript />
          <IconButton>
            <ShareIcon />
          </IconButton>
          <ToggleButton
            className={classes.editorToggle}
            selected={editorOpen}
            onChange={toggleEditor}
            value='editorOpened'
          >
            <EditIcon />
          </ToggleButton>
          <IconButton onClick={toggleCommandOpen}>
            <MagicIcon />
          </IconButton>
        </div>
        <div className={classes.main}>
          <div className={clsx(classes.editor, editorOpen && 'opened')}>
            <Editor />
          </div>
          <div className={classes.canvas}>
            <Canvas />
          </div>
        </div>
        <Drawer open={drawerOpen} onClose={toggleDrawer}>
          <div>drawer content</div>
        </Drawer>
        <CommandInput open={commandOpen} onClose={toggleCommandOpen} />
      </div >
    </LiveContextProvider >
  );
};

export default LiveView;
