/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';

import { Button, Tooltip } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { PixieAPIClient, PixieAPIContext } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { PlayIcon, StopIcon } from 'app/components';
import { EditorContext } from 'app/context/editor-context';
import { ResultsContext } from 'app/context/results-context';
import { ScriptContext } from 'app/context/script-context';
import { GQLClusterStatus } from 'app/types/schema';

const useStyles = makeStyles(({ breakpoints, typography, shape }: Theme) => createStyles({
  buttonText: {
    fontWeight: typography.fontWeightBold,
    [breakpoints.down('md')]: {
      display: 'none',
    },
  },
  buttonContainer: {
    height: '100%',
  },
  buttonRoot: {
    height: '100%',
    borderRadius: shape.borderRadius,
  },
}), { name: 'ExecuteScriptButton' });

const CANCELLABILITY_DELAY_MS = 1000;

const ExecuteScriptButton: React.FC = React.memo(() => {
  const classes = useStyles();
  const cloudClient = (React.useContext(PixieAPIContext) as PixieAPIClient).getCloudClient();
  const { loading: clusterLoading, selectedClusterStatus } = React.useContext(ClusterContext);
  const { loading: resultsLoading, streaming } = React.useContext(ResultsContext);
  const { saveEditor } = React.useContext(EditorContext);
  const { cancelExecution } = React.useContext(ScriptContext);

  const [cancellable, setCancellable] = React.useState<boolean>(false);
  const [cancellabilityTimer, setCancellabilityTimer] = React.useState<number>(undefined);

  const healthy = cloudClient && (selectedClusterStatus === GQLClusterStatus.CS_HEALTHY
    || selectedClusterStatus === GQLClusterStatus.CS_DEGRADED);

  React.useEffect(() => {
    window.clearTimeout(cancellabilityTimer);
    if ((resultsLoading || streaming) && healthy) {
      setCancellabilityTimer(window.setTimeout(() => {
        setCancellable((resultsLoading || streaming) && healthy);
      }, CANCELLABILITY_DELAY_MS));
    } else {
      setCancellable(false);
    }

    // cancellabilityTimer must not appear in this hook's deps. Infinite loop.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [resultsLoading, streaming, healthy, cancelExecution]);

  const tooltipTitle = React.useMemo(() => {
    if (resultsLoading || streaming) return 'Executing';
    if (!healthy) return 'Cluster Disconnected';
    return 'Execute script';
  }, [resultsLoading, streaming, healthy]);

  return (
    <Tooltip title={tooltipTitle}>
      <div className={classes.buttonContainer}>
        <Button
          classes={React.useMemo(() => ({ root: classes.buttonRoot }), [classes.buttonRoot])}
          variant={cancellable ? 'outlined' : 'contained'}
          color='primary'
          disabled={clusterLoading || !healthy || ((resultsLoading || streaming) && !cancellable)}
          onClick={cancellable ? cancelExecution : saveEditor}
          size='small'
          startIcon={cancellable ? <StopIcon /> : <PlayIcon />}
        >
          <span className={classes.buttonText}>{cancellable ? 'Stop' : 'Run'}</span>
        </Button>
      </div>
    </Tooltip>
  );
});
ExecuteScriptButton.displayName = 'ExecuteScriptButton';

export default ExecuteScriptButton;
