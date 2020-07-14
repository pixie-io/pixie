import { scrollbarStyles } from 'common/mui-theme';
import VizierGRPCClientContext from 'common/vizier-grpc-client-context';
import MoveIcon from '@material-ui/icons/OpenWith';
import PixieCommandIcon from 'components/icons/pixie-command';
import { ClusterInstructions } from 'containers/App/deploy-instructions';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import ChevronLeft from '@material-ui/icons/ChevronLeft';
import MoreVertIcon from '@material-ui/icons/MoreVert';

import clsx from 'clsx';
import Canvas from 'containers/live/canvas';
import CommandInput from 'containers/command-input/command-input';
import NewCommandInput from 'containers/new-command-input/new-command-input';
import { withLiveViewContext } from 'containers/live/context';
import { LayoutContext } from 'context/layout-context';
import { ScriptContext } from 'context/script-context';
import { DataDrawerSplitPanel } from 'containers/data-drawer/data-drawer';
import { EditorSplitPanel } from 'containers/editor/editor';
import ExecuteScriptButton from 'containers/live/execute-button';
import { ScriptLoader } from 'containers/live/script-loader';
import LiveViewShortcuts from 'containers/live/shortcuts';
import LiveViewTitle from 'containers/live/title';
import LiveViewBreadcrumbs from 'containers/live/breadcrumbs';
import NavBars from 'containers/App/nav-bars';
import ProfileMenu from 'containers/profile-menu/profile-menu';
import Menu from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
    backgroundColor: theme.palette.background.default,
    color: theme.palette.text.primary,
    ...scrollbarStyles(theme),
  },
  content: {
    marginLeft: theme.spacing(6),
    display: 'flex',
    flex: 1,
    minWidth: 0,
    minHeight: 0,
    flexDirection: 'column',
  },
  title: {
    marginLeft: theme.spacing(2),
    flexGrow: 1,
    color: theme.palette.foreground.grey5,
  },
  mainPanel: {
    flex: 1,
    minHeight: 0,
  },
  moveWidgetToggle: {
    border: 'none',
    borderRadius: '50%',
    color: theme.palette.action.active,
  },
  editorPanel: {
    display: 'flex',
    flexDirection: 'row',
    minHeight: 0,
  },
  canvas: {
    overflowY: 'auto',
    overflowX: 'hidden',
    marginLeft: theme.spacing(0.5),
    height: '100%',
    width: '100%',
  },
  opener: {
    position: 'absolute',
    top: theme.spacing(10) + 2, // Topbar height + border
    height: theme.spacing(6),
    width: theme.spacing(3),
    display: 'flex',
    alignItems: 'center',
    background: theme.palette.background.three,
    cursor: 'pointer',
    borderTopRightRadius: 0,
    borderBottomRightRadius: 0,
    right: 0,
  },
  hidden: {
    display: 'none',
  },
  kabobIcon: {
    color: theme.palette.foreground.three,
  },
  pixieIcon: {
    color: theme.palette.primary.main,
  },
}));

export const EditorOpener = () => {
  const { editorPanelOpen, isMobile, setEditorPanelOpen } = React.useContext(LayoutContext);
  const openEditor = () => setEditorPanelOpen(true);
  const classes = useStyles();

  if (isMobile) {
    return null;
  }

  return (
    <Tooltip title='Open Editor' placement='left'>
      <IconButton disabled={isMobile || editorPanelOpen} className={classes.opener} onClick={openEditor}>
        <ChevronLeft />
      </IconButton>
    </Tooltip>
  );
};

const LiveView = () => {
  const classes = useStyles();

  const { pxl, id, saveEditorAndExecute } = React.useContext(ScriptContext);
  const { loading } = React.useContext(VizierGRPCClientContext);
  const {
    setDataDrawerOpen, setEditorPanelOpen, isMobile,
  } = React.useContext(LayoutContext);

  const [widgetsMoveable, setWidgetsMoveable] = React.useState(false);

  const [commandOpen, setCommandOpen] = React.useState<boolean>(false);
  const toggleCommandOpen = React.useCallback(() => setCommandOpen((opened) => !opened), []);

  const [moreMenuOpen, setMoreMenuOpen] = React.useState<boolean>(false);
  const [anchorEl, setAnchorEl] = React.useState(null);

  const openMenu = React.useCallback((event) => {
    setMoreMenuOpen(true);
    setAnchorEl(event.currentTarget);
  }, []);

  const closeMenu = React.useCallback(() => {
    setMoreMenuOpen(false);
    setAnchorEl(null);
  }, []);

  const hotkeyHandlers = {
    'pixie-command': toggleCommandOpen,
    'toggle-editor': () => setEditorPanelOpen((editable) => !editable),
    'toggle-data-drawer': () => setDataDrawerOpen((open) => !open),
    execute: () => saveEditorAndExecute(),
  };

  React.useEffect(() => {
    if (!pxl && !id) {
      setCommandOpen(true);
    }
  }, [id, pxl]);

  React.useEffect(() => {
    if (isMobile) {
      setWidgetsMoveable(false);
    }
  }, [isMobile]);

  const canvasRef = React.useRef<HTMLDivElement>(null);

  return (
    <div className={classes.root}>
      <LiveViewShortcuts handlers={hotkeyHandlers} />
      <NavBars>
        <LiveViewTitle className={classes.title} />
        <Tooltip title='Pixie Command'>
          <IconButton disabled={commandOpen} onClick={toggleCommandOpen}>
            <PixieCommandIcon fontSize='large' className={classes.pixieIcon} />
          </IconButton>
        </Tooltip>
        <ExecuteScriptButton />
        <Tooltip title='More' onClick={openMenu}>
          <IconButton>
            <MoreVertIcon className={classes.kabobIcon} />
          </IconButton>
        </Tooltip>
        <Menu
          open={moreMenuOpen}
          onClose={closeMenu}
          anchorEl={anchorEl}
          getContentAnchorEl={null}
          anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
        >
          <MenuItem key='edit' button onClick={() => setWidgetsMoveable(!widgetsMoveable)}>
            <ListItemIcon>
              <MoveIcon />
            </ListItemIcon>
            <ListItemText primary={widgetsMoveable ? 'Disable Edit View' : 'Enable Edit View'} />
          </MenuItem>
        </Menu>
        <ProfileMenu />
      </NavBars>
      {
        loading ? <div className='center-content'><ClusterInstructions message='Connecting to cluster...' /></div>
          : (
            <div className={classes.content}>
              <LiveViewBreadcrumbs />
              <ScriptLoader />
              <DataDrawerSplitPanel className={classes.mainPanel}>
                <EditorSplitPanel className={classes.editorPanel}>
                  <div className={classes.canvas} ref={canvasRef}>
                    <Canvas editable={widgetsMoveable} parentRef={canvasRef} />
                  </div>
                </EditorSplitPanel>
              </DataDrawerSplitPanel>
              { localStorage.getItem('px-new-autocomplete') === 'true'
                ? <NewCommandInput open={commandOpen} onClose={toggleCommandOpen} />
                : <CommandInput open={commandOpen} onClose={toggleCommandOpen} /> }
              <EditorOpener />
            </div>
          )
      }
    </div>
  );
};

export default withLiveViewContext(LiveView);
