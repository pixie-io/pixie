import clsx from 'clsx';
import {DARK_THEME} from 'common/mui-theme';
import * as React from 'react';
import {GlobalHotKeys} from 'react-hotkeys';

import Drawer from '@material-ui/core/Drawer';
import IconButton from '@material-ui/core/IconButton';
import {createStyles, makeStyles, Theme, ThemeProvider} from '@material-ui/core/styles';
import EditIcon from '@material-ui/icons/Edit';
import InputIcon from '@material-ui/icons/Input';
import MenuIcon from '@material-ui/icons/Menu';
import ShareIcon from '@material-ui/icons/Share';
import ToggleButton from '@material-ui/lab/ToggleButton';

import Canvas from './canvas';
import CommandInput from './command-input';
import demoSpec from './demo';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: theme.palette.background.default,
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
      margin: theme.spacing(2),
    },
    editorToggle: {
      border: 'none',
    },
    editor: {
      flex: 1,
      minWidth: 0,
      display: 'none',
      '&.editor-open': {
        display: 'block',
      },
    },
    canvas: {
      flex: 1,
      minWidth: 0,
    },
  }));

const COMMAND_KEYMAP = {
  PIXIE_COMMAND: ['Meta+k', 'Control+k'],
};

const LiveView = () => {
  const [drawerOpen, setDrawerOpen] = React.useState<boolean>(false);
  const toggleDrawer = (open: boolean) => () => { setDrawerOpen(open); };

  const [editorOpen, setEditorOpen] = React.useState<boolean>(false);
  const toggleEditor = React.useCallback(() => setEditorOpen((opened) => !opened), []);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((open) => !open), []);

  const classes = useStyles();

  const hotkeyHandlers = React.useMemo(() => ({
    PIXIE_COMMAND: toggleCommandOpen,
  }), []);

  return (
    <div className={classes.root}>
      <GlobalHotKeys handlers={hotkeyHandlers} keyMap={COMMAND_KEYMAP} />
      <div className={classes.topBar}>
        <IconButton onClick={toggleDrawer(true)}>
          <MenuIcon />
        </IconButton>
        <div className={classes.title}>title goes here</div>
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
          <InputIcon />
        </IconButton>
      </div>
      <div className={classes.main}>
        <div className={clsx(classes.editor, editorOpen && 'editor-open')}>
          editor goes here
        </div>
        <div className={classes.canvas}>
          <Canvas spec={demoSpec} />
        </div>
      </div>
      <Drawer open={drawerOpen} onClose={toggleDrawer(false)}>
        <div>drawer content</div>
      </Drawer>
      <CommandInput open={commandOpen} onClose={toggleCommandOpen} />
    </div >
  );
};

export default () => (
  // Wrap the component with the theme provider here for now.
  // TODO(malthus): Remove this once the whole app switches to MUI.
  <ThemeProvider theme={DARK_THEME}>
    <LiveView />
  </ThemeProvider>
);
