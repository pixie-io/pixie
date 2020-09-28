import ClientContext from 'common/vizier-grpc-client-context';
import PlayIcon from 'components/icons/play';
import * as React from 'react';

import Tooltip from '@material-ui/core/Tooltip';

import { ResultsContext } from 'context/results-context';
import { ScriptContext } from 'context/script-context';
import {
  Button, createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';

const styles = ({ breakpoints, typography }: Theme) => createStyles({
  buttonText: {
    fontWeight: typography.fontWeightBold,
    [breakpoints.down('md')]: {
      display: 'none',
    },
  },
});

const StyledButton = withStyles((theme: Theme) => createStyles({
  root: {
    height: theme.spacing(4.25),
    borderRadius: `0 ${theme.shape.borderRadius}px ${theme.shape.borderRadius}px 0px`,
  },
}))(Button);

type ExecuteScriptButtonProps = WithStyles<typeof styles>;

const ExecuteScriptButtonBare = ({ classes }: ExecuteScriptButtonProps) => {
  const { healthy } = React.useContext(ClientContext);
  const { loading } = React.useContext(ResultsContext);
  const { saveEditorAndExecute } = React.useContext(ScriptContext);

  let tooltipTitle;
  if (loading) {
    tooltipTitle = 'Executing';
  } else if (!healthy) {
    tooltipTitle = 'Cluster Disconnected';
  } else {
    tooltipTitle = 'Execute script';
  }

  return (
    <Tooltip title={tooltipTitle}>
      <div>
        <StyledButton
          variant='contained'
          color='primary'
          disabled={!healthy || loading}
          onClick={saveEditorAndExecute}
          size='small'
          startIcon={<PlayIcon />}
        >
          <span className={classes.buttonText}>Run</span>
        </StyledButton>
      </div>
    </Tooltip>
  );
};

const ExecuteScriptButton = withStyles(styles)(ExecuteScriptButtonBare);
export default ExecuteScriptButton;
