import ClientContext from 'common/vizier-grpc-client-context';
import PlayIcon from 'components/icons/play';
import * as React from 'react';

import Tooltip from '@material-ui/core/Tooltip';

import { ResultsContext } from 'context/results-context';
import { ScriptContext } from 'context/script-context';
import {
  Button, createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';

const styles = ({ breakpoints }: Theme) => createStyles({
  buttonText: {
    [breakpoints.down('md')]: {
      display: 'none',
    },
  },
});

type ExecuteScriptButtonProps = WithStyles<typeof styles>;

const ExecuteScriptButtonBare = ({ classes }: ExecuteScriptButtonProps) => {
  const { healthy } = React.useContext(ClientContext);
  const { loading } = React.useContext(ResultsContext);
  const { saveEditorAndExecute } = React.useContext(ScriptContext);

  return (
    <Tooltip title={loading ? 'Executing' : !healthy ? 'Cluster Disconnected' : 'Execute script'}>
      <div>
        <Button
          variant='contained'
          color='primary'
          disabled={!healthy || loading}
          onClick={saveEditorAndExecute}
          size='small'
          startIcon={<PlayIcon />}
        >
          <span className={classes.buttonText}>Run</span>
        </Button>
      </div>
    </Tooltip>
  );
};

const ExecuteScriptButton = withStyles(styles)(ExecuteScriptButtonBare);
export default ExecuteScriptButton;
