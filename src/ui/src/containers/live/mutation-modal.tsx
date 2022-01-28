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

import {
  Button,
  Card,
  CircularProgress,
  Grid,
  ListItem,
  ListItemIcon,
  ListItemText,
  Modal,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { StatusCell } from 'app/components';
import { ScriptContext } from 'app/context/script-context';
import { MutationInfo, LifeCycleState } from 'app/types/generated/vizierapi_pb';
import * as moonwalkerSVG from 'assets/images/moonwalker.svg';

const useStyles = makeStyles(({ spacing, typography, palette }: Theme) => createStyles({
  mutationCard: {
    width: '40%',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    flexDirection: 'column',
    padding: spacing(2),
    paddingBottom: spacing(3),
    transform: 'translate(65%, 20vh)',
  },
  cardHeader: {
    ...typography.h6,
    color: palette.foreground.two,
    paddingBottom: spacing(2),
    display: 'flex',
    justifyContent: 'center',
  },
  image: {
    width: spacing(16),
  },
  icon: {
    minWidth: spacing(4),
  },
  mutation: {
    paddingTop: 0,
    paddingBottom: 0,
  },
  schema: {
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    paddingLeft: spacing(5),
  },
  leftStates: {
    '& > li': {
      paddingLeft: spacing(5),
    },
  },
  cancelButton: {
    display: 'flex',
    flexDirection: 'row-reverse',
    alignSelf: 'flex-end',
    paddingBottom: spacing(1),
  },
  schemaText: {
    flex: 'none',
  },
}), { name: 'MutationModal' });

interface MutationModalProps {
  mutationInfo: MutationInfo;
}

const MutationState = React.memo<{ state?: LifeCycleState }>(({ state }) => {
  switch (state) {
    case LifeCycleState.RUNNING_STATE:
      return (<StatusCell statusGroup='healthy' />);
    case LifeCycleState.FAILED_STATE:
      return (<StatusCell statusGroup='unhealthy' />);
    default:
      break;
  }

  return <CircularProgress size={18} />;
});
MutationState.displayName = 'MutationState';

const MutationModal = React.memo<MutationModalProps>(({ mutationInfo }) => {
  const classes = useStyles();
  const { cancelExecution } = React.useContext(ScriptContext);

  return (
    <Modal open>
      <Card className={classes.mutationCard}>
        <Grid container spacing={3}>
          <Grid item xs={12}>
            <div className={classes.cardHeader}>
              Deploying Tracepoints
            </div>
          </Grid>
          <Grid item xs={6} className={classes.leftStates}>
            {
               mutationInfo.getStatesList().slice(0, Math.ceil(mutationInfo.getStatesList().length)).map((mutation) => (
                 <ListItem className={classes.mutation} key={mutation.getId()}>
                   <ListItemIcon className={classes.icon}>
                     <MutationState state={mutation.getState()} />
                   </ListItemIcon>
                   <ListItemText>{mutation.getName()}</ListItemText>
                 </ListItem>
               ))
            }
          </Grid>
          <Grid item xs={6}>
            {
              mutationInfo.getStatesList().slice(Math.ceil(mutationInfo.getStatesList().length)).map((mutation) => (
                <ListItem className={classes.mutation} key={mutation.getId()}>
                  <ListItemIcon className={classes.icon}>
                    <MutationState state={mutation.getState()} />
                  </ListItemIcon>
                  <ListItemText>{mutation.getName()}</ListItemText>
                </ListItem>
              ))
            }
          </Grid>
          <Grid item xs={12}>
            {
              (true || mutationInfo.getStatus().getMessage().includes('Schema'))
              && (
                <ListItem className={classes.schema} key='schema'>
                  <ListItemIcon className={classes.icon}>
                    <MutationState state={LifeCycleState.PENDING_STATE} />
                  </ListItemIcon>
                  <ListItemText className={classes.schemaText}>Prepare schema</ListItemText>
                </ListItem>
              )
            }
          </Grid>
          <Grid item xs={6}>
            <img alt='' className={classes.image} src={moonwalkerSVG} />
          </Grid>
          <Grid item xs={6} className={classes.cancelButton}>
            <Button onClick={cancelExecution}>
              Cancel
            </Button>
          </Grid>
        </Grid>
      </Card>
    </Modal>
  );
});
MutationModal.displayName = 'MutationModal';

export default MutationModal;
