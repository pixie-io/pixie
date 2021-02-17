import ClientContext from 'common/vizier-grpc-client-context';
import { PlayIcon, StopIcon } from '@pixie/components';
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
  buttonContainer: {
    height: '100%',
  },
});

const StyledButton = withStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    borderRadius: `0 ${theme.shape.borderRadius}px ${theme.shape.borderRadius}px 0px`,
  },
}))(Button);

type ExecuteScriptButtonProps = WithStyles<typeof styles>;

const CANCELLABILITY_DELAY_MS = 1000;

const ExecuteScriptButtonBare = ({ classes }: ExecuteScriptButtonProps) => {
  const { healthy } = React.useContext(ClientContext);
  const { loading, streaming } = React.useContext(ResultsContext);
  const { saveEditorAndExecute, cancelExecution } = React.useContext(ScriptContext);

  const [cancellable, setCancellable] = React.useState<boolean>(false);
  const [cancellabilityTimer, setCancellabilityTimer] = React.useState<number>(undefined);

  React.useEffect(() => {
    window.clearTimeout(cancellabilityTimer);
    if ((loading || streaming) && healthy) {
      setCancellabilityTimer(window.setTimeout(() => {
        setCancellable((loading || streaming) && healthy);
      }, CANCELLABILITY_DELAY_MS));
    } else {
      setCancellable(false);
    }

    // cancellabilityTimer must not appear in this hook's deps. Infinite loop.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [loading, streaming, healthy, cancelExecution]);

  let tooltipTitle;
  if (loading || streaming) {
    tooltipTitle = 'Executing';
  } else if (!healthy) {
    tooltipTitle = 'Cluster Disconnected';
  } else {
    tooltipTitle = 'Execute script';
  }

  return (
    <Tooltip title={tooltipTitle}>
      <div className={classes.buttonContainer}>
        <StyledButton
          variant={cancellable ? 'outlined' : 'contained'}
          color='primary'
          disabled={!healthy || ((loading || streaming) && !cancellable)}
          onClick={cancellable ? cancelExecution : saveEditorAndExecute}
          size='small'
          startIcon={cancellable ? <StopIcon /> : <PlayIcon />}
        >
          <span className={classes.buttonText}>{cancellable ? 'Stop' : 'Run'}</span>
        </StyledButton>
      </div>
    </Tooltip>
  );
};

const ExecuteScriptButton = withStyles(styles)(ExecuteScriptButtonBare);
export default ExecuteScriptButton;
